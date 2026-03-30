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

#include <SSLCertManager.h>

// Обязательно для Windows: связывает CRT приложения с CRT внутри OpenSSL DLL.
// Должен быть включён ровно в один .cpp файл проекта.
#include <openssl/applink.c>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>

#include <memory>

// ---------------------------------------------------------------------------
// RAII-обёртки для OpenSSL-ресурсов.
// Каждый deleter — отдельная структура (не лямбда), чтобы тип unique_ptr
// был именованным и читаемым в отладчике.
// ---------------------------------------------------------------------------
namespace {

struct EvpPkeyCtxDeleter { void operator()(EVP_PKEY_CTX* p) const { EVP_PKEY_CTX_free(p); } };
struct EvpPkeyDeleter    { void operator()(EVP_PKEY* p)     const { EVP_PKEY_free(p); } };
struct X509Deleter       { void operator()(X509* p)         const { X509_free(p); } };
struct BioDeleter        { void operator()(BIO* p)          const { BIO_free(p); } };
struct BioChainDeleter   { void operator()(BIO* p)          const { BIO_free_all(p); } };
struct OpenSslFreeDeleter{ void operator()(void* p)         const { OPENSSL_free(p); } };

using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;
using EvpPkeyPtr    = std::unique_ptr<EVP_PKEY,     EvpPkeyDeleter>;
using X509Ptr       = std::unique_ptr<X509,         X509Deleter>;
using BioPtr        = std::unique_ptr<BIO,           BioDeleter>;      // одиночный BIO
using BioChainPtr   = std::unique_ptr<BIO,           BioChainDeleter>; // голова цепочки BIO

// Читает всё содержимое memory BIO в std::string.
// bio должен быть BIO_s_mem(), данные должны быть записаны и сброшены перед вызовом.
std::string BioToString(BIO* bio) {
    char* data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    if (len <= 0 || !data) return {};
    return std::string(data, static_cast<std::size_t>(len));
}

} // namespace

// ---------------------------------------------------------------------------

bool SSLCertManager::generate(const std::string& ip, CertKeyPair& out) {
    out = {};

    // --- Генерация RSA-2048 ключа ---
    EvpPkeyCtxPtr pctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    if (!pctx) return false;
    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) return false;
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx.get(), 2048) <= 0) return false;

    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_keygen(pctx.get(), &raw_pkey) <= 0) return false;
    EvpPkeyPtr pkey(raw_pkey);

    // --- Создание X.509 v3 сертификата ---
    X509Ptr x509(X509_new());
    if (!x509) return false;

    X509_set_version(x509.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1);

    // Срок действия: сейчас + 10 лет
    X509_gmtime_adj(X509_getm_notBefore(x509.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(x509.get()), 10L * 365 * 24 * 3600);

    X509_set_pubkey(x509.get(), pkey.get());

    // Субъект (самоподписанный — субъект = издатель)
    X509_NAME* name = X509_get_subject_name(x509.get());
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("RU"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("PhoneBarcodeScanner"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("PhoneBarcodeScanner"), -1, -1, 0);
    X509_set_issuer_name(x509.get(), name);

    // Subject Alternative Name с IP-адресом сервера.
    // Обязательно по RFC 2818: Android проверяет SAN, игнорируя CN.
    {
        char san_value[64];
        snprintf(san_value, sizeof(san_value), "IP:%s", ip.c_str());
        X509V3_CTX v3ctx;
        X509V3_set_ctx_nodb(&v3ctx);
        X509V3_set_ctx(&v3ctx, x509.get(), x509.get(), nullptr, nullptr, 0);
        X509_EXTENSION* san_ext = X509V3_EXT_conf_nid(
            nullptr, &v3ctx, NID_subject_alt_name, san_value);
        if (san_ext) {
            X509_add_ext(x509.get(), san_ext, -1);
            X509_EXTENSION_free(san_ext);
        }
    }

    // Подписываем SHA-256
    if (!X509_sign(x509.get(), pkey.get(), EVP_sha256())) return false;

    // --- Вычисляем SHA-256 отпечаток публичного ключа (SPKI) ---
    // Формат OkHttp CertificatePinner: "sha256/<base64(SHA256(DER(SPKI)))>"
    {
        unsigned char* raw_spki = nullptr;
        const int spki_len = i2d_PUBKEY(pkey.get(), &raw_spki);

        // raw_spki выделен OpenSSL — оборачиваем для автоматического освобождения
        std::unique_ptr<unsigned char, OpenSslFreeDeleter> spki_der(raw_spki);

        if (spki_len > 0 && spki_der) {
            unsigned char digest[EVP_MAX_MD_SIZE];
            unsigned int digest_len = 0;
            EVP_Digest(spki_der.get(), static_cast<std::size_t>(spki_len),
                       digest, &digest_len, EVP_sha256(), nullptr);

            // Base64 без переносов строк.
            // Создаём цепочку: b64 (фильтр) → mem (приёмник).
            // После BIO_push(b64, mem) голова цепочки — b64, она владеет mem.
            // BioChainPtr вызовет BIO_free_all(b64), что освободит и b64, и mem.
            BIO* mem_raw = BIO_new(BIO_s_mem());
            if (mem_raw) {
                BIO* b64_raw = BIO_new(BIO_f_base64());
                if (b64_raw) {
                    BIO_set_flags(b64_raw, BIO_FLAGS_BASE64_NO_NL);
                    BIO_push(b64_raw, mem_raw);         // b64 берёт владение mem
                    BioChainPtr b64_chain(b64_raw);     // RAII: BIO_free_all при выходе
                    BIO_write(b64_chain.get(), digest, static_cast<int>(digest_len));
                    BIO_flush(b64_chain.get());
                    // mem_raw — хвост цепочки, по-прежнему доступен для чтения
                    out.certPin = "sha256/" + BioToString(mem_raw);
                } else {
                    BIO_free(mem_raw);  // b64 не создан — освобождаем mem вручную
                }
            }
        }
    }

    // --- Записываем приватный ключ в memory BIO ---
    BioPtr key_bio(BIO_new(BIO_s_mem()));
    if (!key_bio) return false;
    if (PEM_write_bio_PrivateKey(key_bio.get(), pkey.get(),
                                  nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        return false;
    }
    out.keyPem = BioToString(key_bio.get());

    // --- Записываем сертификат в memory BIO ---
    BioPtr cert_bio(BIO_new(BIO_s_mem()));
    if (!cert_bio) return false;
    if (PEM_write_bio_X509(cert_bio.get(), x509.get()) != 1) {
        return false;
    }
    out.certPem = BioToString(cert_bio.get());

    return true;
}
