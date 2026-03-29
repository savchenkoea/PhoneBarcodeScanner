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

#ifndef WSSERVER_WSIOTHREAD_H
#define WSSERVER_WSIOTHREAD_H

#include <mutex>
#include "WSInclude.h"

class IOThread
{
public:
    // В поле threadId хранится идентификатор потока.
    // Значение присваивается при создании потока и далее используется для поиска потока
    int threadId;

    // сигнал о получении данных
    signals2::signal<void(int, std::string)> sigOnDataReceiving;

    // сигнал о закрытии соединения
    signals2::signal<void(int)> sigOnClosedConnection;

    // конструктор экземпляра класса
    IOThread(int id, tcp::socket socket, ssl::context &ctx, beast::error_code &ec);

    // деструктор экземпляра класса
    ~IOThread();

    // функция останавливает работу потока чтения из веб-сокета
    int close();

    // функция возвращает признак того, что поток работает
    bool running() const;
    
    // функция отправляет сообщение mes в связанный с потоком веб-сокет
    int send(const std::string &mes);

    // функция возвращает ip адрес клиента
    std::string getClientIp() const;

    // функция возвращает удаленный порт клиента
    int getClientPort() const;

    // функция возвращает признак того, что клиент авторизован
    bool authenticated() const;

    // функция устанавливает признак того, что клиент авторизован
    void setAuthenticated(bool value);

private:
    // веб-сокет, через который производится обмен данными в данном потоке
    websocket::stream<beast::ssl_stream<tcp::socket>> ws;

    // ссылка на поток, принимающий сообщения из веб-сокета
    std::thread t;

    // флаг завершения работы потока
    std::atomic<bool> stopThread{false};

    // флаг работы потока
    std::atomic<bool> isRunning{false};

    // мьютекс для защиты записи в сокет
    std::mutex writeMutex;

    // ip адрес клиента
    std::string client_ip;

    // удаленный порт клиента
    int client_port;

    // флаг авторизации клиента
    std::atomic<bool> isAuthenticated{false};


    // процедура потока, принимающего сообщения из веб-сокета
    void threadRoutine();
};
#endif WSSERVER_WSIOTHREAD_H