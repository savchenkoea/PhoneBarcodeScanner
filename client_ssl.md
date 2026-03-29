# План: переход с ws:// на wss:// (защищённое соединение)

**Дата:** Март 2026
**Проект:** barcode2pc — Android-клиент

---

## Анализ текущего состояния

**Проблема:** `WebSocketRepositoryImpl.kt:88` строит URL `ws://${config.ip}:${config.port}` — нешифрованное соединение. Данные (passkey, штрих-коды) передаются в открытом виде.

**Ключевая сложность:** Сервер — десктопное приложение на IP-адресе локальной сети. Оно почти гарантированно использует самоподписанный сертификат. Стандартный `TrustManager` Android отклонит его. Простая замена `ws://` на `wss://` приведёт к `SSLHandshakeException`.

---

## Выбор подхода

| Вариант | Плюсы | Минусы |
|---|---|---|
| Доверять всем сертификатам (`trustAllCerts`) | Просто | Полностью небезопасно — MITM-атака возможна |
| Встроить сертификат в APK | Просто | Нельзя — сертификат сервера у каждого пользователя разный |
| **Отпечаток (fingerprint) через QR-код** | Безопасно, без CA, масштабируемо | Нужно изменить QR-формат |
| Android `network_security_config` с custom CA | Стандартно | Не работает динамически (один IP на все установки) |

**Выбранный подход: Certificate Pinning с доставкой отпечатка через QR-код.**

**Логика:** Пользователь сканирует QR-код с экрана сервера. Тот, кто видит экран сервера — имеет физический доступ к машине. Включение SHA-256 отпечатка публичного ключа в QR — это безопасная форма TOFU (Trust On First Use), привязанная к конкретному сертификату.

---

## Изменения в QR-коде

**До:**
```json
{"ip": "192.168.1.10", "port": 8765, "passkey": "secret"}
```

**После:**
```json
{"ip": "192.168.1.10", "port": 8765, "passkey": "secret", "certPin": "sha256/AbCdEf...=="}
```

Поле `certPin` — Base64-кодированный SHA-256 хэш публичного ключа сертификата сервера (формат OkHttp CertificatePinner: `"sha256/..."`, ~44 символа). QR-код с таким объёмом данных уверенно читается камерой.

---

## Список изменений в коде клиента (Android)

### 1. `domain/model/ConnectionConfig.kt`
Добавить поле `certPin: String?` (необязательное):
```kotlin
data class ConnectionConfig(
    val ip: String,
    val port: Int,
    val passkey: String,
    val certPin: String? = null   // "sha256/..." или null
)
```

### 2. `domain/usecase/ParseConnectionQrUseCase.kt`
Читать опциональное поле `certPin` из JSON. Отсутствие поля — не ошибка.

### 3. `data/repository/WebSocketRepositoryImpl.kt`
- Строка 88: `"ws://${config.ip}:${config.port}"` → `"wss://${config.ip}:${config.port}"`
- Передать `config.certPin` в `dataSource.connect()`

### 4. `data/remote/WebSocketDataSource.kt`
Изменить сигнатуру `connect`:
```kotlin
fun connect(url: String, certPin: String? = null)
```
При наличии `certPin` — создать производный `OkHttpClient` с настроенным `CertificatePinner`:
```kotlin
val client = if (certPin != null) {
    okHttpClient.newBuilder()
        .certificatePinner(
            CertificatePinner.Builder()
                .add(hostname, certPin)
                .build()
        )
        .build()
} else {
    okHttpClient
}
```
Здесь `hostname` — IP-адрес, извлечённый из URL. OkHttp поддерживает IP-адреса в `CertificatePinner`.

### 5. `di/NetworkModule.kt`
Без изменений. Базовый `OkHttpClient` — синглтон. Производный клиент с `CertificatePinner` создаётся в `WebSocketDataSource.connect()` через `newBuilder()`.

### 6. `data/local/ConnectionConfigStorage.kt`
Сохранять и загружать поле `certPin` в `EncryptedSharedPreferences` (рядом с `ip`, `port`, `passkey`).

### 7. `AndroidManifest.xml`
Убедиться, что отсутствует `android:usesCleartextTraffic="true"`. В `targetSdk=35` cleartext уже запрещён по умолчанию, кроме localhost.

---

## Требования к серверу

Для поддержки защищённого соединения сервер должен:

### 1. Сгенерировать самоподписанный сертификат
Сертификат должен содержать IP-адрес сервера в поле Subject Alternative Name (SAN):
```
SubjectAltName: IP:192.168.1.10
```
Без SAN с IP Android отклонит сертификат при TLS-handshake (RFC 2818).

Пример генерации через OpenSSL:
```bash
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
  -days 3650 -nodes \
  -subj "/CN=barcode2pc-server" \
  -addext "subjectAltName=IP:192.168.1.10"
```

Для динамического IP (адрес может меняться) — добавить все ожидаемые адреса или использовать `IP:0.0.0.0` (не рекомендуется).

### 2. Запустить WSS-сервер
Переключить WebSocket-сервер с `ws://` на `wss://`, подключив сгенерированный сертификат и ключ.

Примеры:

**Python (websockets):**
```python
import ssl
import websockets

ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.load_cert_chain("server.crt", "server.key")

async with websockets.serve(handler, "0.0.0.0", 8765, ssl=ssl_context):
    await asyncio.Future()
```

**Node.js (ws):**
```javascript
const fs = require('fs');
const https = require('https');
const WebSocket = require('ws');

const server = https.createServer({
    cert: fs.readFileSync('server.crt'),
    key: fs.readFileSync('server.key')
});
const wss = new WebSocket.Server({ server });
server.listen(8765);
```

### 3. Вычислить SHA-256 отпечаток публичного ключа
Именно публичного ключа (SPKI), а не самого сертификата. Это формат OkHttp `CertificatePinner`.

```bash
openssl x509 -in server.crt -pubkey -noout \
  | openssl pkey -pubin -outform DER \
  | openssl dgst -sha256 -binary \
  | openssl base64
```

Результат (пример): `AbCdEfGhIjKlMnOpQrStUvWxYz0123456789ABCD=`

### 4. Включить отпечаток в QR-код авторизации
Сформировать JSON с полем `certPin`:
```json
{
  "ip": "192.168.1.10",
  "port": 8765,
  "passkey": "your-secret-key",
  "certPin": "sha256/AbCdEfGhIjKlMnOpQrStUvWxYz0123456789ABCD="
}
```
Обратите внимание на префикс `sha256/` — он обязателен для формата OkHttp.

---

## Схема потока данных

```
QR-код сервера
  {"ip":..., "port":..., "passkey":..., "certPin":"sha256/..."}
         ↓
ParseConnectionQrUseCase
  → ConnectionConfig(ip, port, passkey, certPin)
         ↓
AuthenticateUseCase → WebSocketRepositoryImpl.connect(config)
  url = "wss://${ip}:${port}"
  certPin передаётся в WebSocketDataSource.connect(url, certPin)
         ↓
WebSocketDataSource
  OkHttpClient = baseClient.newBuilder()
    .certificatePinner(hostname, certPin)
    .build()
  client.newWebSocket(request, listener)
         ↓
TLS Handshake:
  ① Шифрование канала (TLS 1.2/1.3)
  ② Верификация: SHA-256(serverPublicKey) == certPin → OK
         ↓
Зашифрованное соединение установлено
```

---

## Что НЕ нужно менять на клиенте

- `IWebSocketRepository` — интерфейс не меняется (`connect(config: ConnectionConfig)`)
- `AuthViewModel`, `ScanViewModel` — без изменений
- `WsMessage`, `WsEvent` — без изменений
- Синглтон `OkHttpClient` в `NetworkModule` — без изменений (производный клиент создаётся на лету)
- Room, DI-граф — без изменений

---

## Обратная совместимость

Поле `certPin` в `ConnectionConfig` — nullable. Если QR-код не содержит `certPin`:
- Соединение устанавливается по `wss://` без пиннинга
- Канал зашифрован, но подлинность сервера не проверяется (приемлемо для доверенной локальной сети)

---

## Итоговый список изменений

### Клиент (Android) — 6 файлов кода
| Файл | Изменение |
|---|---|
| `domain/model/ConnectionConfig.kt` | +1 поле `certPin: String?` |
| `domain/usecase/ParseConnectionQrUseCase.kt` | парсинг нового поля |
| `data/repository/WebSocketRepositoryImpl.kt` | `ws://` → `wss://`, передача `certPin` |
| `data/remote/WebSocketDataSource.kt` | новый параметр `certPin`, динамический `CertificatePinner` |
| `data/local/ConnectionConfigStorage.kt` | сохранение/чтение `certPin` |
| `AndroidManifest.xml` | проверить `usesCleartextTraffic` |

### Сервер
| Задача | Описание |
|---|---|
| Генерация сертификата | `openssl req` с SAN для IP-адреса сервера |
| WSS-сервер | Подключить TLS к WebSocket-серверу |
| Вычисление отпечатка | SHA-256 публичного ключа в Base64 (формат SPKI) |
| QR-код | Добавить поле `certPin` в JSON |
