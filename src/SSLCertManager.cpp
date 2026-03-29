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

#include "../include/SSLCertManager.h"

// Обязательно для Windows: связывает CRT приложения с CRT внутри OpenSSL DLL.
// Должен быть включён ровно в один .cpp файл проекта.
#include <openssl/applink.c>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>

#include <cstdio>

// Читает всё содержимое memory BIO в std::string
static std::string bioToString(BIO *bio)
{
    char *data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    if (len <= 0 || !data) return {};
    return std::string(data, static_cast<size_t>(len));
}

bool SSLCertManager::generate(const std::string &ip, CertKeyPair &out)
{
    out = {};

    // --- Генерация RSA-2048 ключа ---
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!pctx) return false;

    bool ok = false;

    if (EVP_PKEY_keygen_init(pctx) <= 0) goto cleanup_pctx;
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) goto cleanup_pctx;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) goto cleanup_pctx;

    {
        // --- Создание X.509 сертификата ---
        X509 *x509 = X509_new();
        if (!x509) goto cleanup_pctx;

        // X.509 v3
        X509_set_version(x509, 2);

        // Серийный номер
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

        // Срок действия: сейчас + 10 лет
        X509_gmtime_adj(X509_getm_notBefore(x509), 0);
        X509_gmtime_adj(X509_getm_notAfter(x509), 10L * 365 * 24 * 3600);

        // Публичный ключ
        X509_set_pubkey(x509, pkey);

        // Субъект (самоподписанный — субъект = издатель)
        X509_NAME *name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
            reinterpret_cast<const unsigned char *>("RU"), -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
            reinterpret_cast<const unsigned char *>("PhoneBarcodeScanner"), -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char *>("PhoneBarcodeScanner"), -1, -1, 0);
        X509_set_issuer_name(x509, name);

        // --- Subject Alternative Name с IP-адресом сервера ---
        // Обязательно по RFC 2818: Android проверяет SAN, игнорируя CN.
        {
            char sanValue[64];
            snprintf(sanValue, sizeof(sanValue), "IP:%s", ip.c_str());
            X509V3_CTX v3ctx;
            X509V3_set_ctx_nodb(&v3ctx);
            X509V3_set_ctx(&v3ctx, x509, x509, nullptr, nullptr, 0);
            X509_EXTENSION *sanExt = X509V3_EXT_conf_nid(
                nullptr, &v3ctx, NID_subject_alt_name, sanValue);
            if (sanExt) {
                X509_add_ext(x509, sanExt, -1);
                X509_EXTENSION_free(sanExt);
            }
        }

        // Подписываем SHA-256
        if (!X509_sign(x509, pkey, EVP_sha256()))
        {
            X509_free(x509);
            goto cleanup_pctx;
        }

        // --- Вычисляем SHA-256 отпечаток публичного ключа (SPKI) ---
        // Формат OkHttp CertificatePinner: "sha256/<base64(SHA256(DER(SPKI)))>"
        {
            unsigned char *spkiDer = nullptr;
            int spkiLen = i2d_PUBKEY(pkey, &spkiDer);
            if (spkiLen > 0 && spkiDer)
            {
                unsigned char digest[EVP_MAX_MD_SIZE];
                unsigned int digestLen = 0;
                EVP_Digest(spkiDer, static_cast<size_t>(spkiLen),
                           digest, &digestLen, EVP_sha256(), nullptr);
                OPENSSL_free(spkiDer);

                // Base64 без переносов строк
                BIO *b64 = BIO_new(BIO_f_base64());
                BIO *mem = BIO_new(BIO_s_mem());
                BIO_push(b64, mem);
                BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
                BIO_write(b64, digest, static_cast<int>(digestLen));
                BIO_flush(b64);
                out.certPin = "sha256/" + bioToString(mem);
                BIO_free_all(b64);
            }
        }

        // --- Записываем приватный ключ в memory BIO ---
        BIO *keyBio = BIO_new(BIO_s_mem());
        if (!keyBio)
        {
            X509_free(x509);
            goto cleanup_pctx;
        }
        ok = PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr) == 1;
        if (ok) out.keyPem = bioToString(keyBio);
        BIO_free(keyBio);

        if (!ok)
        {
            X509_free(x509);
            goto cleanup_pctx;
        }

        // --- Записываем сертификат в memory BIO ---
        BIO *certBio = BIO_new(BIO_s_mem());
        if (!certBio)
        {
            X509_free(x509);
            ok = false;
            goto cleanup_pctx;
        }
        ok = PEM_write_bio_X509(certBio, x509) == 1;
        if (ok) out.certPem = bioToString(certBio);
        BIO_free(certBio);

        X509_free(x509);
    }

cleanup_pctx:
    EVP_PKEY_CTX_free(pctx);
    if (pkey) EVP_PKEY_free(pkey);
    return ok;
}
