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

#include <random>
#include <algorithm>
#include "../include/WSErrors.h"
#include "../include/WSServerThread.h"
#include "../include/SSLCertManager.h"
#include "../include/NetworkUtils.h"

WSServerThread::WSServerThread() {
    this->isRunning.store(false);
    this->generateNewPasskey();
}


WSServerThread::~WSServerThread() {
    this->stop();
}

// Функция проверяет корректность переданного в функцию ip адреса
static int addressIsValid(const std::string &ipAddress) {

    if (std::empty(ipAddress)) {
        return ERROR_SERVER_ADDRESS_IS_NOT_SPECIFIED;
    }

    boost::system::error_code ec;
    ip::make_address_v4(ipAddress, ec);

    if (ec) {
        return ERROR_INVALID_SERVER_ADDRESS;
    }

    return ERROR_NO_ERROR;
}

// Функция проверяет корректность переданного в функцию номера порта
static int portIsValid(const int &port)
{
    if (port == 0) return ERROR_SERVER_PORT_IS_NOT_SPECIFIED;
    if (port < 0 || port > 65535) return ERROR_INVALID_SERVER_PORT_NUMBER;

    return ERROR_NO_ERROR;
}

// Функция запускает сервер
int WSServerThread::run(const std::string &address, const int &port)
{
    // Проверяем переданный в функцию адрес сервера

    int cr = addressIsValid(address);
    if (cr != ERROR_NO_ERROR) return cr;

    // Проверяем переданный в функцию номер порта сервера
    cr = portIsValid(port);
    if (cr != ERROR_NO_ERROR) return cr;

// Проверяем, что адрес входит в список доступных адресов на компьютере
    std::vector<std::string> activeAddresses = NetworkUtils::GetActiveIPv4Addresses();
    if (std::find(activeAddresses.begin(), activeAddresses.end(), address) == activeAddresses.end())
    {
        return ERROR_INVALID_SERVER_ADDRESS;
    }

    if (NetworkUtils::IsPortBusy(address, port)) {
        return ERROR_PORT_IS_ALREADY_IN_USE;
    }

    if (this->running()) return ERROR_SERVER_IS_ALREADY_RUNNING;

    try
    {
        boost::system::error_code ec;

        // Генерируем самоподписанный сертификат в памяти.
        // Передаём address, чтобы IP попал в SAN — иначе Android отклонит сертификат.
        SSLCertManager::CertKeyPair ckp;
        if (!SSLCertManager::generate(address, ckp)) return ERROR_SSL_CERT_GENERATE_FAILED;
        currentCertPin = ckp.certPin;

        // Загружаем сертификат и ключ в SSL-контекст из памяти (без файлов)
        sslCtx.use_certificate_chain(boost::asio::buffer(ckp.certPem), ec);
        if (ec) return ERROR_SSL_CERT_LOAD_FAILED;

        sslCtx.use_private_key(boost::asio::buffer(ckp.keyPem), ssl::context::pem, ec);
        if (ec) return ERROR_SSL_KEY_LOAD_FAILED;

        stopThread.store(false);

        // преобразуем строку в адрес ipv4
        const auto addressIP = ip::make_address(address, ec);
        if (ec) return ERROR_LIB_MAKE_ADDRESS;

        // endpoint хранит информацию о точке подключения (адрес и порт)
        const tcp::endpoint endpoint(addressIP, port);

        // открываем acceptor используя протокол из точки подключения endpoint
        acceptor.open(endpoint.protocol(), ec); // NOLINT(*-unused-return-value)
        if (ec) return ERROR_LIB_OPEN;

        // устанавливаем дополнительные параметры acceptor
        acceptor.set_option(net::socket_base::reuse_address(true), ec); // NOLINT(*-unused-return-value)
        if (ec) return ERROR_LIB_SET_OPTION;

        // связываем acceptor с точкой подключения endpoint
        acceptor.bind(endpoint, ec); // NOLINT(*-unused-return-value)
        if (ec) return ERROR_LIB_BIND;

        // разрешаем прием входящих соединений на acceptor.
        // max_listen_connections - максимальное количество соединений в очереди
        acceptor.listen(net::socket_base::max_listen_connections, ec); // NOLINT(*-unused-return-value)
        if (ec) return ERROR_LIB_LISTEN;

        // запускаем поток, который будет принимать входящие соединения
        // ссылка на поток хранится в поле t

        this->t = std::thread(&WSServerThread::serverThread, this);

    } catch ([[maybe_unused]] const std::exception& ex)
    {
        return ERROR_SERVER_THREAD_STARTUP_ERROR;
    }

    // возвращаем успешный результат
    return ERROR_NO_ERROR;
}

// функция возвращает текущий passkey
std::string WSServerThread::getPasskey() const
{
    return this->currentPasskey;
}

// функция возвращает SHA-256 отпечаток публичного ключа текущего сертификата
std::string WSServerThread::getCertPin() const
{
    return this->currentCertPin;
}

// функция генерирует новый passkey
void WSServerThread::generateNewPasskey()
{
    static constexpr char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string newPasskey;
    newPasskey.reserve(64);
    for (int i = 0; i < 64; ++i) {
        newPasskey += charset[dis(gen)];
    }

    this->currentPasskey = newPasskey;
    sigPasskeyChanged(this->currentPasskey);
}

// Основной поток сервера
void WSServerThread::serverThread()
{
    try
    {
        boost::system::error_code ec;

        // устанавливаем флаг isRunning, что основной поток работает
        this->isRunning.store(true);

        // повторяем пока не будет установлен флан stopThread
        while (!stopThread.load()) {

            // создаем socket для приема входящего соединения
            tcp::socket socket{ioc};

            // останавливаем работу потока до приема соединения
            acceptor.accept(socket, ec); // NOLINT(*-unused-return-value)

            // если был установлен флаг завершения работы, то завершаем цикл приема соединений
            if (stopThread.load()) {
                break;
            }

            // если соединение успешно установлено
            if (!ec) {
                {
                    // блокируем другие операции с вектором потоков чтения
                    std::lock_guard lock(this->threadsMutex);

                    // создаем новый экземпляр класса IOThread.
                    // конструктор класса запускает новый поток, который будет осуществлять
                    // чтение информации их веб-сокета, созданного на основе сокета socket.

                    threads.emplace_back(std::make_unique<IOThread>(++nextThreadId,
                        std::move(socket), sslCtx, ec));

                    if (!ec)
                    {
                        IOThread* newThread = threads.back().get();
                        newThread->sigOnDataReceiving.connect([this](int id, std::string data) {
                            this->sigOnDataReceiving(id, data);
                        });
                        newThread->sigOnClosedConnection.connect([this](int id) {
                            this->sigOnClosedConnection(id);
                        });
                        sigOnNewConnection(newThread->threadId, newThread->getClientIp(), newThread->getClientPort());
                    }
                }
            }
        }
        // сбрасываем флаг, что сервер работает
        this->isRunning.store(false);

    } catch ([[maybe_unused]] const std::exception &ex)
    {
    }

}

// Функция останавливает сервер
int WSServerThread::stop() {
    if (this->running())
    {
        boost::system::error_code ec;

        // устанавливаем флаг завершения работы всех обработчиков
        stopThread.store(true);

        // удаляем все ранее созданные потоки чтения
        this->closeAll();

        // Отменяем все ожидающие операции acceptor
        // Это разблокирует accept() в serverThread
        acceptor.cancel(ec);

        // закрываем acceptor
        acceptor.close(ec);


        // Ждём завершения потока
        if (this->t.joinable()) {
            this->t.join();
        }

        if (ec) return ERROR_LIB_CLOSE;

        return ERROR_NO_ERROR;
    }
    return ERROR_SERVER_HAS_ALREADY_BEEN_STOPPED;
}

// Функция возвращает значение флага isRunning
bool WSServerThread::running() const
{
    if (this->isRunning.load()) return true;
    return false;
}

int WSServerThread::send(const int id, const std::string& mes)
{
    // блокируем работу с вектором потоков
    std::lock_guard lock(this->threadsMutex);

    auto it = std::find_if(threads.begin(), threads.end(),
                           [id](const std::unique_ptr<IOThread>& ti) {
                               return ti->threadId == id; // Условие поиска
                           });

    // если поток не найден, то возвращаем код ошибки
    if (it == threads.end()) return ERROR_THREAD_NOT_FOUND;


    // если поток найден, но не активен, то возвращаем код ошибки
    if (!it->get()->running()) return ERROR_THREAD_NOT_ACTIVE;

    // если поток с идентификатором threatId найден, тогда отправляем через него сообщение mes
    return it->get()->send(mes);
}

int WSServerThread::close(const int id)
{
    // блокируем работу с вектором потоков
    std::lock_guard lock(this->threadsMutex);

    auto it = std::find_if(threads.begin(), threads.end(),
                           [id](const std::unique_ptr<IOThread>& ti) {
                               return ti->threadId == id; // Условие поиска
                           });

    // если поток не найден, то возвращаем код ошибки
    if (it == threads.end()) return ERROR_THREAD_NOT_FOUND;

    // если поток с идентификатором id найден, тогда закрываем его
    int result = it->get()->close();

    // удаляем из вектора информацию о потоке
    threads.erase(it);

    return result;
}


int WSServerThread::closeAll()
{
    int result = ERROR_NO_ERROR;

    std::lock_guard lock(this->threadsMutex);

    for (auto& ti : threads) result = std::max(result, ti->close());

    // очищаем вектор потоков
    threads.clear();

    return result;
}

int WSServerThread::getConnectionsList(std::vector<WSConnectionInfo>& connections_list)
{
    std::lock_guard lock(this->threadsMutex);
    connections_list.clear();

    for (const auto& ti : threads)
    {
        if (ti->running())
        {
            WSConnectionInfo info;
            info.id = ti->threadId;
            info.ip = ti->getClientIp();
            info.port = ti->getClientPort();
            connections_list.push_back(info);
        }
    }

    return ERROR_NO_ERROR;
}

int WSServerThread::getConnectionInfo(const int id, WSConnectionInfo &info)
{
    if (id == 0) return ERROR_THREAD_ID_IS_NOT_SPECIFIED;
    if (id < 0) return ERROR_INVALID_THREAD__ID;
    
    std::lock_guard lock(this->threadsMutex);
    
    for (const auto& ti : threads) {
        if (ti->threadId == id) {
            if (!ti->running()) return ERROR_THREAD_NOT_ACTIVE;
            else
            {
                info.id = ti->threadId;
                info.ip = ti->getClientIp();
                info.port = ti->getClientPort();
                return ERROR_NO_ERROR;
            }
        }
    }
    return ERROR_THREAD_NOT_FOUND;
}

bool WSServerThread::isAuthenticated(int id)
{
    std::lock_guard lock(this->threadsMutex);
    for (const auto& ti : threads) {
        if (ti->threadId == id) {
            return ti->authenticated();
        }
    }
    return false;
}

void WSServerThread::setAuthenticated(int id, bool value)
{
    std::lock_guard lock(this->threadsMutex);
    for (const auto& ti : threads) {
        if (ti->threadId == id) {
            ti->setAuthenticated(value);
            break;
        }
    }
}