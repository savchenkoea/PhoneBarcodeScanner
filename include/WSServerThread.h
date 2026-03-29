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

#ifndef WSSERVER_THREAD_H
#define WSSERVER_THREAD_H

#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <string>

#include "WSIOThread.h"

class WSConnectionInfo {
public:
    int id;
    std::string ip;
    int port;
};

class WSServerThread{
public:
    WSServerThread();

    ~WSServerThread();

    // сигнал о новом подключении
    signals2::signal<void(int, std::string, int)> sigOnNewConnection;

    // сигнал о закрытии подключении
    signals2::signal<void(int)> sigOnClosedConnection;

    // сигнал о получении данных
    signals2::signal<void(int, std::string)> sigOnDataReceiving;

    // функция возвращает true, если сервер запущен
    // и false, если сервер остановлен
    bool running() const;

    // функция запускает сервер
    int run(const std::string &addressVariant, const int &portVariant);

    // функция останавливает сервер
    int stop();

    // функция отправляет сообщение mes в связанный с потоком threatId веб-сокет
    int send(int id, const std::string& mes);

    // функция закрывает веб-сокет, связанный с потоком threatId
    int close(int id);

    // функция закрывает все веб-сокеты
    int closeAll();

    // функция возвращает список активных соединений
    int WSServerThread::getConnectionsList(std::vector<WSConnectionInfo>& connections_list);

    // функция возвращает информацию о соединении
    int getConnectionInfo(int id, WSConnectionInfo &info);

    // функция проверяет авторизацию клиента
    bool isAuthenticated(int id);

    // функция устанавливает статус авторизации клиента
    void setAuthenticated(int id, bool value);

    // сигнал об изменении passkey
    signals2::signal<void(std::string)> sigPasskeyChanged;

    // функция возвращает текущий passkey
    std::string getPasskey() const;

    // функция генерирует новый passkey
    void generateNewPasskey();

    // функция возвращает SHA-256 отпечаток публичного ключа текущего сертификата
    // в формате OkHttp CertificatePinner: "sha256/<base64>"
    std::string getCertPin() const;

private:
    // Внутренние свойства объекта:

    // флаг работы сервера
    std::atomic<bool> isRunning{false};

    // флаг завершения работы сервера
    std::atomic<bool> stopThread{false};

    // ссылка на основной поток сервера
    std::thread t;

    // вектор ссылок на экземпляры класса IOThread.
    // класс IOThread описывает поток чтения сообщений из веб-сокета.
    std::vector<std::unique_ptr<IOThread>> threads;

    // мьютекс управляет очередностью операций с вектором потоков чтения сообщений
    std::mutex threadsMutex;

    // io_context необходим для всех операций ввода/вывода
    net::io_context ioc;

    // acceptor принимает входящие соединения
    tcp::acceptor acceptor{ioc};

    // SSL-контекст, общий для всех соединений сервера
    ssl::context sslCtx{ssl::context::tls_server};

    // В переменной nextThreadId хранится идентификатор следующего потока ввода-вывода.
    // Начинаем с 1 и для каждого следующего потока увеличиваем на 1
    int nextThreadId{0};

    // Текущий passkey для авторизации клиентов
    std::string currentPasskey;

    // SHA-256 отпечаток публичного ключа текущего сертификата (формат OkHttp)
    std::string currentCertPin;

    // Внутренние методы объекта:

    void serverThread();

};

#endif //WSSERVER_THREAD_H
