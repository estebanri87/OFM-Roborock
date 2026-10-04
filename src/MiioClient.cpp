#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

// KNX-frei halten: hier darf kein OpenKNX-/knx-Header hinein.
#include "MiioClient.h"
#include <Arduino.h>
#include <errno.h>
#include <esp_random.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // close()

namespace
{
    constexpr uint16_t MIIO_PORT = 54321;
    constexpr uint16_t MIIO_MAGIC = 0x2131;

    // Gesamtbudget je Aufruf. Wird nur über millis() geprüft, nie gewartet.
    constexpr uint32_t TRANSACTION_TIMEOUT_MS = 5000;
    // Hello ist zustandslos und darf wiederholt werden, die Anfrage nicht: Roborock
    // verwirft eine bereits gesehene Message-ID.
    constexpr uint32_t HELLO_RESEND_MS = 1000;
    // Danach wird vor dem nächsten Aufruf neu angemeldet (Zeitstempel nachziehen).
    constexpr uint32_t HANDSHAKE_MAX_AGE_MS = 120000;
    // Nach einem Timeout überspringt die nächste ID diesen Abstand (wie python-miio).
    constexpr uint32_t ID_SKIP_AFTER_TIMEOUT = 100;
    // python-miio hält die IDs unter 9999; Geräte sind darauf eingestellt.
    constexpr uint32_t ID_MAX = 9999;

    void put16(uint8_t* p, uint16_t v)
    {
        p[0] = (uint8_t)(v >> 8);
        p[1] = (uint8_t)v;
    }

    void put32(uint8_t* p, uint32_t v)
    {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    }

    uint16_t get16(const uint8_t* p)
    {
        return (uint16_t)((p[0] << 8) | p[1]);
    }

    uint32_t get32(const uint8_t* p)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }

    // Über die generische md-API, damit es nicht von der mbedTLS-Hauptversion abhängt.
    void md5(const uint8_t* data, size_t len, uint8_t out[16])
    {
        mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_MD5), data, len, out);
    }

    int hexValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
} // namespace

MiioClient::MiioClient()
{
    // Zufälliger Start, damit sich die IDs nicht mit einem zweiten Client (z.B. Home
    // Assistant, beginnt bei 1) überschneiden.
    _nextId = 1000 + (esp_random() % 8000);
}

MiioClient::~MiioClient()
{
    closeSocket();
}

bool MiioClient::configure(const char* ip, const char* tokenHex)
{
    _configured = false;
    closeSocket();
    _handshakeValid = false;
    if (ip == nullptr || tokenHex == nullptr) return false;

    struct in_addr probe;
    if (inet_pton(AF_INET, ip, &probe) != 1) return false;
    strncpy(_ip, ip, sizeof(_ip) - 1);
    _ip[sizeof(_ip) - 1] = 0;

    if (strlen(tokenHex) != 32) return false;
    for (uint8_t i = 0; i < 16; i++)
    {
        const int hi = hexValue(tokenHex[2 * i]);
        const int lo = hexValue(tokenHex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        _token[i] = (uint8_t)((hi << 4) | lo);
    }

    md5(_token, 16, _key);
    uint8_t keyToken[32];
    memcpy(keyToken, _key, 16);
    memcpy(keyToken + 16, _token, 16);
    md5(keyToken, 32, _iv);

    _configured = true;
    return true;
}

bool MiioClient::beginCall(const char* method, const char* params)
{
    if (!_configured) return false;
    if (busy()) return false;

    const int n = snprintf(_plain, sizeof(_plain), "{\"id\":%lu,\"method\":\"%s\",\"params\":%s}",
                           (unsigned long)_nextId, method, params ? params : "[]");
    if (n <= 0 || (size_t)n >= sizeof(_plain))
    {
        _result = ErrTooLong;
        _state = Complete;
        return true;
    }
    _plainLen = (size_t)n + 1; // wie python-miio: abschließendes Nullbyte mitverschlüsseln
    _sentId = _nextId;
    _nextId = _nextId >= ID_MAX ? 1 : _nextId + 1;
    strncpy(_method, method, sizeof(_method) - 1);
    _method[sizeof(_method) - 1] = 0;
    _response[0] = 0;
    _result = Pending;
    _deadline = millis() + TRANSACTION_TIMEOUT_MS;

    if (_sock < 0 && !openSocket())
    {
        fail(ErrSocket);
        return true;
    }

    const bool stale = (millis() - _stampAtMs) > HANDSHAKE_MAX_AGE_MS;
    _state = (_handshakeValid && !stale) ? SendRequest : SendHello;
    return true;
}

void MiioClient::clear()
{
    if (_state == Complete) _state = Idle;
}

void MiioClient::poll()
{
    if (_state == Idle || _state == Complete) return;

    if ((int32_t)(millis() - _deadline) >= 0)
    {
        // Gerät antwortet nicht: beim nächsten Mal neu anmelden und eine
        // deutlich höhere ID verwenden, falls die alte doch angekommen ist.
        _handshakeValid = false;
        _nextId += ID_SKIP_AFTER_TIMEOUT;
        if (_nextId >= ID_MAX) _nextId -= ID_MAX - 1;
        fail(ErrTimeout);
        return;
    }

    switch (_state)
    {
        case SendHello:
            sendHello();
            return;

        case WaitHello:
        {
            const int n = receivePacket();
            if (n >= 0) handleHello((size_t)n);
            if (_state == WaitHello && (millis() - _helloSentMs) >= HELLO_RESEND_MS)
                _state = SendHello;
            return;
        }

        case SendRequest:
            sendRequest();
            return;

        case WaitReply:
        {
            const int n = receivePacket();
            if (n < 0) return;
            handleReply((size_t)n);
            return;
        }

        default:
            return;
    }
}

bool MiioClient::openSocket()
{
    _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_sock < 0) return false;

    const int flags = fcntl(_sock, F_GETFL, 0);
    if (flags < 0 || fcntl(_sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        closeSocket();
        return false;
    }
    return true;
}

void MiioClient::closeSocket()
{
    if (_sock >= 0)
    {
        close(_sock);
        _sock = -1;
    }
}

bool MiioClient::sendPacket(const uint8_t* data, size_t len)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MIIO_PORT);
    inet_pton(AF_INET, _ip, &addr.sin_addr);

    const int n = sendto(_sock, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (n == (int)len) return true;
    if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) return false; // nächster Durchlauf
    closeSocket();
    fail(ErrSocket);
    return false;
}

// Liefert die Länge eines Pakets vom Gerät, -1 wenn (noch) nichts da ist.
int MiioClient::receivePacket()
{
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    const int n = recvfrom(_sock, _packet, sizeof(_packet), 0, (struct sockaddr*)&from, &fromLen);
    if (n < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            closeSocket();
            fail(ErrSocket);
        }
        return -1;
    }

    struct in_addr expected;
    inet_pton(AF_INET, _ip, &expected);
    if (from.sin_addr.s_addr != expected.s_addr) return -1; // fremdes Paket
    return n;
}

void MiioClient::sendHello()
{
    uint8_t hello[HEADER_LEN];
    memset(hello, 0xFF, sizeof(hello));
    put16(hello, MIIO_MAGIC);
    put16(hello + 2, HEADER_LEN);
    if (!sendPacket(hello, sizeof(hello))) return;
    _helloSentMs = millis();
    _state = WaitHello;
}

void MiioClient::handleHello(size_t len)
{
    if (len < HEADER_LEN || get16(_packet) != MIIO_MAGIC) return;
    _deviceId = get32(_packet + 8);
    _deviceStamp = get32(_packet + 12);
    _stampAtMs = millis();
    _handshakeValid = true;
    _state = SendRequest;
}

void MiioClient::sendRequest()
{
    // PKCS#7: immer 1..16 Byte auffüllen.
    const size_t padded = (_plainLen / 16 + 1) * 16;
    uint8_t plain[PLAIN_CAP + 16];
    memcpy(plain, _plain, _plainLen);
    memset(plain + _plainLen, (int)(padded - _plainLen), padded - _plainLen);

    uint8_t* out = _packet;
    uint8_t iv[16];
    memcpy(iv, _iv, 16);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, _key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded, iv, plain, out + HEADER_LEN);
    mbedtls_aes_free(&aes);

    const uint32_t stamp = _deviceStamp + (millis() - _stampAtMs) / 1000 + 1;
    put16(out, MIIO_MAGIC);
    put16(out + 2, (uint16_t)(HEADER_LEN + padded));
    put32(out + 4, 0);
    put32(out + 8, _deviceId);
    put32(out + 12, stamp);
    memcpy(out + 16, _token, 16); // Prüfsumme wird über Kopf mit Token gebildet
    uint8_t checksum[16];
    md5(out, HEADER_LEN + padded, checksum);
    memcpy(out + 16, checksum, 16);

    if (!sendPacket(out, HEADER_LEN + padded)) return;
    _state = WaitReply;
}

void MiioClient::handleReply(size_t len)
{
    if (len < HEADER_LEN || get16(_packet) != MIIO_MAGIC) return;
    if (get16(_packet + 2) != len)
    {
        fail(ErrFrame);
        return;
    }
    const size_t cipherLen = len - HEADER_LEN;
    if (cipherLen == 0) return; // Hello-Antwort o.ä. - weiter warten
    if (cipherLen % 16 != 0)
    {
        fail(ErrFrame);
        return;
    }
    if (cipherLen > RESPONSE_CAP)
    {
        fail(ErrTooLong);
        return;
    }

    uint8_t received[16];
    memcpy(received, _packet + 16, 16);
    memcpy(_packet + 16, _token, 16);
    uint8_t checksum[16];
    md5(_packet, len, checksum);
    if (memcmp(received, checksum, 16) != 0)
    {
        fail(ErrChecksum);
        return;
    }

    uint8_t iv[16];
    memcpy(iv, _iv, 16);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, _key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, _packet + HEADER_LEN, (uint8_t*)_response);
    mbedtls_aes_free(&aes);

    const uint8_t pad = (uint8_t)_response[cipherLen - 1];
    if (pad == 0 || pad > 16)
    {
        _response[0] = 0;
        fail(ErrDecrypt);
        return;
    }
    size_t plainLen = cipherLen - pad;
    while (plainLen > 0 && _response[plainLen - 1] == 0)
        plainLen--; // manche Firmwares hängen Nullbytes an
    _response[plainLen] = 0;

    // Antwort zu einer älteren Anfrage (z.B. nach Timeout) ignorieren. Die ID steht am
    // Ende ({"result":...,"id":N}); das letzte Vorkommen nehmen, falls das Ergebnis
    // selbst Objekte mit "id" enthält.
    const char* idField = nullptr;
    for (const char* p = strstr(_response, "\"id\":"); p != nullptr; p = strstr(p + 1, "\"id\":"))
        idField = p;
    if (idField == nullptr || strtoul(idField + 5, nullptr, 10) != _sentId) return;

    _deviceStamp = get32(_packet + 12);
    _stampAtMs = millis();
    _result = strstr(_response, "\"error\"") != nullptr ? ErrDevice : Ok;
    _state = Complete;
}

void MiioClient::fail(Result result)
{
    _result = result;
    _state = Complete;
}

const char* MiioClient::resultText(Result result)
{
    switch (result)
    {
        case Ok:
            return "OK";
        case Pending:
            return "läuft";
        case ErrNotConfigured:
            return "nicht konfiguriert";
        case ErrBusy:
            return "beschäftigt";
        case ErrSocket:
            return "Socket-Fehler";
        case ErrTimeout:
            return "keine Antwort";
        case ErrFrame:
            return "ungültiges Paket";
        case ErrChecksum:
            return "Prüfsumme falsch (Token?)";
        case ErrDecrypt:
            return "Entschlüsselung fehlgeschlagen (Token?)";
        case ErrDevice:
            return "Gerät meldet Fehler";
        case ErrTooLong:
            return "Nachricht zu lang";
    }
    return "?";
}

#endif // OPENKNX_ROBOROCK
