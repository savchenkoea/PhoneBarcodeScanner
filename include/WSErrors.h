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

#ifndef WSSERVER_ERRORS_H
#define WSSERVER_ERRORS_H

constexpr int ERROR_NO_ERROR = 0;
constexpr int ERROR_LIB_ERROR = 10;
constexpr int ERROR_LIB_MAKE_ADDRESS = 11;
constexpr int ERROR_LIB_OPEN = 12;
constexpr int ERROR_LIB_SET_OPTION = 13;
constexpr int ERROR_LIB_BIND = 14;
constexpr int ERROR_LIB_LISTEN = 15;
constexpr int ERROR_LIB_CLOSE = 16;

constexpr int ERROR_INCORRECT_SERVER_ADDRESS_TYPE = 100;
constexpr int ERROR_INCORRECT_SERVER_PORT_TYPE = 101;
constexpr int ERROR_INCORRECT_THREAD_ID_TYPE = 102;
constexpr int ERROR_INCORRECT_MESSAGE_TYPE = 103;
constexpr int ERROR_INVALID_SERVER_ADDRESS = 104;
constexpr int ERROR_INVALID_SERVER_PORT_NUMBER = 105;
constexpr int ERROR_INVALID_THREAD__ID = 106;
constexpr int ERROR_PORT_IS_ALREADY_IN_USE = 107;

constexpr int ERROR_SERVER_ADDRESS_IS_NOT_SPECIFIED = 120;
constexpr int ERROR_SERVER_PORT_IS_NOT_SPECIFIED = 121;
constexpr int ERROR_THREAD_ID_IS_NOT_SPECIFIED = 122;

constexpr int ERROR_SERVER_THREAD_STARTUP_ERROR = 130;
constexpr int ERROR_SERVER_IS_ALREADY_RUNNING = 131;
constexpr int ERROR_SERVER_HAS_ALREADY_BEEN_STOPPED = 132;
constexpr int ERROR_WEBSOCKET_IS_NOT_OPEN = 133;
constexpr int ERROR_THREAD_NOT_FOUND = 134;
constexpr int ERROR_THREAD_NOT_ACTIVE = 135;

constexpr int ERROR_SSL_CERT_LOAD_FAILED = 140;
constexpr int ERROR_SSL_KEY_LOAD_FAILED = 141;
constexpr int ERROR_SSL_CERT_GENERATE_FAILED = 142;

#endif //WSSERVER_ERRORS_H