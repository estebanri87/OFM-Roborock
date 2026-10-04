#pragma once
#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

// KNX-frei halten: hier darf kein OpenKNX-/knx-Header hinein (Muster SolarmanV5Client).
#include <stddef.h>
#include <stdint.h>

// Lokaler miIO-Client (Xiaomi/Roborock, UDP 54321) für genau ein Gerät.
//
// Ablauf eines Aufrufs: Hello-Paket -> Antwort mit Geräte-ID und Zeitstempel ->
// JSON-RPC-Anfrage, AES-128-CBC verschlüsselt (Schlüssel = MD5(Token),
// IV = MD5(Schlüssel + Token)), Kopf mit MD5-Prüfsumme -> verschlüsselte Antwort.
//
// poll() kehrt immer sofort zurück: Socket ist O_NONBLOCK, Zeitlimits laufen nur über
// millis(). Nur IPv4-Literale, Namensauflösung würde blockieren.
class MiioClient
{
  public:
    enum Result : uint8_t
    {
        Ok,
        Pending,
        ErrNotConfigured,
        ErrBusy,
        ErrSocket,
        ErrTimeout,
        ErrFrame,
        ErrChecksum,
        ErrDecrypt,
        ErrDevice, // Gerät meldet {"error":...}
        ErrTooLong,
    };

    MiioClient();
    ~MiioClient();

    // ip: IPv4-Literal, token: 32 Hex-Zeichen. false bei ungültiger Eingabe.
    bool configure(const char* ip, const char* tokenHex);
    bool configured() const { return _configured; }

    // params ist ein JSON-Array als Text, z.B. "[]" oder "[104]".
    bool beginCall(const char* method, const char* params = "[]");
    void poll();

    bool busy() const { return _state != Idle && _state != Complete; }
    bool finished() const { return _state == Complete; }
    Result result() const { return _result; }
    void clear();

    // Entschlüsselte JSON-Antwort des letzten Aufrufs (nur nach finished()).
    const char* response() const { return _response; }
    const char* method() const { return _method; }
    uint32_t deviceId() const { return _deviceId; }
    bool handshakeValid() const { return _handshakeValid; }
    uint32_t lastId() const { return _sentId; }

    static const char* resultText(Result result);

  private:
    enum State : uint8_t
    {
        Idle,
        SendHello,
        WaitHello,
        SendRequest,
        WaitReply,
        Complete,
    };

    static constexpr size_t HEADER_LEN = 32;
    static constexpr size_t PLAIN_CAP = 192;     // Anfrage-JSON
    static constexpr size_t RESPONSE_CAP = 1024; // Antwort-JSON (get_status ~ 450 Byte)

    bool _configured = false;
    char _ip[16] = {0};
    uint8_t _token[16] = {0};
    uint8_t _key[16] = {0};
    uint8_t _iv[16] = {0};

    int _sock = -1;
    State _state = Idle;
    Result _result = Ok;
    uint32_t _deadline = 0;
    uint32_t _helloSentMs = 0;

    bool _handshakeValid = false;
    uint32_t _deviceId = 0;
    uint32_t _deviceStamp = 0;
    uint32_t _stampAtMs = 0;

    uint32_t _nextId = 0;
    uint32_t _sentId = 0;
    char _method[32] = {0};
    char _plain[PLAIN_CAP] = {0};
    size_t _plainLen = 0;

    uint8_t _packet[HEADER_LEN + RESPONSE_CAP + 16];
    char _response[RESPONSE_CAP + 1] = {0};

    bool openSocket();
    void closeSocket();
    bool sendPacket(const uint8_t* data, size_t len);
    int receivePacket();
    void sendHello();
    void sendRequest();
    void handleHello(size_t len);
    void handleReply(size_t len);
    void fail(Result result);
};

#endif // OPENKNX_ROBOROCK
