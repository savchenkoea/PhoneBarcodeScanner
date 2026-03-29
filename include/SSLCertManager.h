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

#pragma once

#include <string>

class SSLCertManager {
public:
    struct CertKeyPair {
        std::string certPem;
        std::string keyPem;
        // SHA-256 отпечаток публичного ключа (SPKI) в формате OkHttp CertificatePinner: "sha256/<base64>"
        std::string certPin;
    };

    // Генерирует самоподписанный RSA-2048 сертификат в памяти.
    // ip — адрес сервера, добавляется в SAN (обязательно для Android RFC 2818).
    // Возвращает true при успехе, out заполняется PEM-данными и отпечатком certPin.
    static bool generate(const std::string &ip, CertKeyPair &out);
};
