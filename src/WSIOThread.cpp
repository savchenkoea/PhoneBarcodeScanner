/*
 *  Web socket server for 1C platform
 *  Copyright (C) 2026 Evgenii Savchenko
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../include/WSErrors.h"
#include "../include/WSIOThread.h"

// Конструктор создает веб-сокет ws на основе сокета socket и запускает поток, которые слушает данный веб-сокет.
// Полученные сообщения передаются в 1С с помощью механизма внешних событий
IOThread::IOThread(const int id, tcp::socket socket,
                   beast::error_code &ec) : threadId(id),
                                            ws(std::move(socket))
{

    // Получаем информацию о подключившемся клиенте
    tcp::endpoint remote_ep = ws.next_layer().remote_endpoint(ec);
    if (!ec)
    {
        client_ip = remote_ep.address().to_string();
        client_port = remote_ep.port();
    }

    // выполняем рукопожатие с клиентом
    this->ws.accept(ec);

    // если произошла ошибка - выходим
    if (ec) {
        return;
    }

    // запускаем поток
    this->t = std::thread(&IOThread::threadRoutine, this);
}

// деструктор экземпляра класса
IOThread::~IOThread()
{
    this->close();
}

// функция останавливает работу потока чтения из веб-сокета
int IOThread::close()
{
    beast::error_code ec;

    // устанавливаем флаг завершения потока чтения
    this->stopThread.store(true);

    // если веб-сокет открыт...
    if (this->ws.is_open())
    {
        // Для прерывания синхронного read() необходимо закрыть сам TCP сокет.
        // cancel() работает только для async операций.
        // ws.close() здесь вызывать опасно, так как сокет занят в другом потоке.

        // Закрываем прием и передачу данных на уровне TCP
        this->ws.next_layer().shutdown(tcp::socket::shutdown_both, ec); // NOLINT(*-unused-return-value)

        // Закрываем дескриптор сокета
        this->ws.next_layer().close(ec); // NOLINT(*-unused-return-value)
    }

    // проверяем что поток еще существует
    if (this->t.joinable())
    {
        // и ожидаем завершения дочернего потока
        this->t.join();
    }

    // устанавливаем признак того, что поток завершил работу
    this->isRunning.store(false);

    if (ec==websocket::error::closed) return ERROR_NO_ERROR;

    if (ec) return ERROR_LIB_ERROR;

    return ERROR_NO_ERROR;
}


// функция проверяет, что поток работает
bool IOThread::running() const
{
    return this->isRunning.load();
}


// функция отправляет сообщение mes в связанный с потоком веб-сокет
int IOThread::send(const std::string &mes)
{
    // Проверяем, открыт ли веб-сокет
    if (!this->ws.is_open())
    {
        return ERROR_WEBSOCKET_IS_NOT_OPEN;
    }

    // Используем локальную переменную ошибки
    beast::error_code ec;

    // Устанавливаем режим отправки текста (WebSocket Text Frame)
    this->ws.text(true);

    // Отправляем данные синхронно.
    // Boost.Beast позволяет одновременное выполнение read (в потоке IOThread)
    // и write (в текущем вызывающем потоке).
    this->ws.write(boost::asio::buffer(mes), ec);

    if (ec)
    {
        return ERROR_LIB_ERROR;
    }

    return ERROR_NO_ERROR;
}

// функция возвращает ip адрес клиента
std::string IOThread::getClientIp() const
{
    return this->client_ip;
}

// функция возвращает удаленный порт клиента
int IOThread::getClientPort() const
{
    return this->client_port;
}

// функция возвращает признак того, что клиент авторизован
bool IOThread::authenticated() const
{
    return this->isAuthenticated.load();
}

// функция устанавливает признак того, что клиент авторизован
void IOThread::setAuthenticated(bool value)
{
    this->isAuthenticated.store(value);
}

// процедура потока, принимающего сообщения из веб-сокета
void IOThread::threadRoutine()
{
    // устанавливаем признак того, что поток работает
    this->isRunning.store(true);

    beast::error_code ec;

    try
    {
        while (!stopThread.load()) {
            // буфер для хранения полученного сообщения
            beast::flat_buffer buffer;

            // получаем в буфер buffer информацию из веб-сокета ws
            this->ws.read(buffer, ec);
            if (stopThread.load())
            {
                break;
            }

            // если произошла ошибка (в том числе из-за закрытия сокета клиентом)
            // тогда прекращаем работу потока
            if (ec) {
                break;
            }

            sigOnDataReceiving(this->threadId, beast::buffers_to_string(buffer.data()));
        }
    } catch ([[maybe_unused]] const std::exception &ex)
    {
    }

    // после выхода из цикла по любой причине

    // если веб-сокет открыт...
    if (this->ws.is_open())
    {
        // закрываем веб-сокет
        this->ws.close(websocket::close_code::normal, ec);
    }

    // устанавливаем признак того, что поток прекратил работу.
    this->isRunning.store(false);

    sigOnClosedConnection(this->threadId);

}
