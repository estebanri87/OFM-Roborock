#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

#include "RoborockChannel.h"
#include "NetworkModule.h"
#include "RoborockModule.h"
#include <ArduinoJson.h>

using namespace Roborock;

namespace
{
    // Failed calls in a row before the robot counts as unreachable. UDP loses the odd
    // packet, a single miss must not flip the status object.
    constexpr uint8_t FAILURES_UNREACHABLE = 3;

    // Consumable lifetimes in seconds, as used by the Mi Home app / python-miio.
    constexpr uint32_t LIFETIME_S[4] = {300UL * 3600, 200UL * 3600, 150UL * 3600, 30UL * 3600};
    const char *const CONSUMABLE_FIELD[4] = {"main_brush_work_time", "side_brush_work_time", "filter_work_time", "sensor_dirty_time"};

    // ETS text parameters are not terminated when filled to the last byte.
    void paramText(const uint8_t *data, size_t size, char *out, size_t outSize)
    {
        const size_t len = strnlen((const char *)data, size);
        const size_t n = len < outSize - 1 ? len : outSize - 1;
        memcpy(out, data, n);
        out[n] = 0;
    }

    // Status codes of the classic Roborock API (python-miio STATE_CODE_TO_STRING).
    const char *stateText(int state)
    {
        switch (state)
        {
            case 1: return "Startet";
            case 2: return "Abgekoppelt";
            case 3: return "Bereit";
            case 4: return "Fernsteuerung";
            case 5: return "Reinigt";
            case 6: return "Fährt zurück";
            case 7: return "Manuell";
            case 8: return "Lädt";
            case 9: return "Ladefehler";
            case 10: return "Pausiert";
            case 11: return "Spotreinigung";
            case 12: return "Fehler";
            case 13: return "Fährt herunter";
            case 14: return "Update";
            case 15: return "Dockt an";
            case 16: return "Fährt zum Ziel";
            case 17: return "Zonenreinigung";
            case 18: return "Raumreinigung";
            case 22: return "Entleert";
            case 100: return "Voll geladen";
            case 101: return "Offline";
        }
        return "Unbekannt";
    }

    // Error codes of the classic Roborock API (python-miio ERROR_CODES), max. 14 characters.
    const char *errorText(int code)
    {
        switch (code)
        {
            case 0: return "Kein Fehler";
            case 1: return "Lasersensor";
            case 2: return "Stoßsensor";
            case 3: return "Rad in der Luft";
            case 4: return "Absturzsensor";
            case 5: return "Hauptbürste";
            case 6: return "Seitenbürste";
            case 7: return "Antriebsrad";
            case 8: return "Festgefahren";
            case 9: return "Behälter fehlt";
            case 10: return "Filter prüfen";
            case 11: return "Magnetband";
            case 12: return "Akku schwach";
            case 13: return "Ladefehler";
            case 14: return "Akkufehler";
            case 15: return "Wandsensor";
            case 16: return "Schräglage";
            case 17: return "Seitenbürste";
            case 18: return "Saugmotor";
            case 19: return "Station o. Str";
            case 20: return "Unbekannt";
            case 21: return "Laserhaube";
            case 22: return "Ladekontakte";
            case 23: return "Station";
            case 24: return "Sperrzone";
            case 254: return "Behälter voll";
            case 255: return "Systemfehler";
        }
        return "Fehler";
    }

    bool isCleaning(int state)
    {
        return state == 5 || state == 11 || state == 16 || state == 17 || state == 18;
    }

    // fan_power -> 0=Leise 1=Standard 2=Stark 3=Max. Current firmware reports the custom
    // modes 101..104, older ones a percentage. -1 = not mappable (e.g. 105 mop mode).
    int fanSpeedLevel(int fanPower)
    {
        if (fanPower >= 101 && fanPower <= 104) return fanPower - 101;
        if (fanPower < 0 || fanPower > 100) return -1;
        if (fanPower <= 38) return 0;
        if (fanPower <= 60) return 1;
        if (fanPower <= 77) return 2;
        return 3;
    }
} // namespace

RoborockChannel::RoborockChannel(uint8_t index, RoborockModule &module)
    : _module(module)
{
    _channelIndex = index;
}

const std::string RoborockChannel::name()
{
    return "Roborock";
}

void RoborockChannel::setup()
{
    char ip[16];
    char token[33];
    paramText(ParamROB_CHIp, 16, ip, sizeof(ip));
    paramText(ParamROB_CHToken, 33, token, sizeof(token));
    _pollSeconds = ParamROB_CHPollInterval;
    _consumableMinutes = ParamROB_CHConsumablePoll;
    _cyclicMinutes = ParamROB_CHCyclicSend;

    if (!_client.configure(ip, token))
        logInfoP("IP-Adresse oder Token ungültig, Kanal inaktiv");
    logDebugP("ip=%s poll=%us consumables=%umin cyclic=%umin", ip, (unsigned)_pollSeconds, (unsigned)_consumableMinutes,
              (unsigned)_cyclicMinutes);
}

void RoborockChannel::loop()
{
    if (!_client.configured()) return;

    _client.poll(); // returns immediately
    if (_client.finished())
    {
        handleFinished();
        _client.clear();
    }

    if (_cyclicMinutes && delayCheck(_lastCyclicMs, _cyclicMinutes * 60000UL))
    {
        _lastCyclicMs = millis();
        resetSendCache();
        _statusDue = true;
        _consumableDue = _consumableMinutes != 0;
    }

    if (!_client.busy()) startNextCall();
}

void RoborockChannel::startNextCall()
{
    if (!openknxNetwork.established()) return;

    // Console and bus commands first, polling only when nothing is waiting.
    if (_console.method[0])
    {
        _client.beginCall(_console.method, _console.params);
        _console.method[0] = 0;
        _pending = PENDING_CONSOLE;
        return;
    }
    if (_queueCount)
    {
        const Command &cmd = _queue[_queueHead];
        _client.beginCall(cmd.method, cmd.params);
        _queueHead = (_queueHead + 1) % QUEUE_SIZE;
        _queueCount--;
        _pending = PENDING_COMMAND;
        return;
    }

    const uint32_t now = millis();
    if (_pollSeconds && (_statusDue || now - _lastStatusMs >= _pollSeconds * 1000UL))
    {
        _statusDue = false;
        _lastStatusMs = now;
        _pending = PENDING_STATUS;
        _client.beginCall("get_status");
        return;
    }
    if (_consumableMinutes && (_consumableDue || now - _lastConsumableMs >= _consumableMinutes * 60000UL))
    {
        _consumableDue = false;
        _lastConsumableMs = now;
        _pending = PENDING_CONSUMABLE;
        _client.beginCall("get_consumable");
    }
}

void RoborockChannel::handleFinished()
{
    const MiioClient::Result result = _client.result();
    const Pending pending = _pending;
    _pending = PENDING_NONE;

    if (pending == PENDING_CONSOLE)
        logInfoP("%s: %s %s", _client.method(), MiioClient::resultText(result), _client.response());
    else if (result != MiioClient::Ok)
        logDebugP("%s: %s %s", _client.method(), MiioClient::resultText(result), _client.response());

    if (result != _lastResult && (result == MiioClient::ErrChecksum || result == MiioClient::ErrDecrypt))
    {
        char diag[16];
        snprintf(diag, sizeof(diag), "K%u Token?", (unsigned)(_channelIndex + 1));
        _module.setDiag(diag);
    }
    _lastResult = result;

    if (result == MiioClient::Ok || result == MiioClient::ErrDevice)
    {
        // The robot answered, even if it rejected the command.
        _failures = 0;
        setReachable(true);
    }
    else if (++_failures >= FAILURES_UNREACHABLE)
        setReachable(false);
    if (result != MiioClient::Ok) return;

    switch (pending)
    {
        case PENDING_STATUS:
            applyStatus(_client.response());
            break;
        case PENDING_CONSUMABLE:
            applyConsumables(_client.response());
            break;
        case PENDING_COMMAND:
            _statusDue = true; // show the effect right away
            if (strcmp(_client.method(), "reset_consumable") == 0) _consumableDue = true;
            break;
        default:
            break;
    }
}

void RoborockChannel::setReachable(bool reachable)
{
    const bool changed = reachable != _reachable;
    _reachable = reachable;
    sendBool(KoROB_CHReachable, reachable, DPT_State, _koReachable);
    if (!changed) return;

    char diag[16];
    snprintf(diag, sizeof(diag), "K%u %s", (unsigned)(_channelIndex + 1), reachable ? "online" : "offline");
    _module.setDiag(diag);
    logInfoP("%s", reachable ? "erreichbar" : "nicht erreichbar");
}

void RoborockChannel::applyStatus(const char *json)
{
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    JsonObjectConst s = doc["result"][0];
    if (s.isNull()) return;

    _state = s["state"] | -1;
    _battery = s["battery"] | -1;
    _fanPower = s["fan_power"] | -1;
    _errorCode = s["error_code"] | -1;

    if (_state >= 0)
    {
        sendInt(KoROB_CHState, _state, DPT_Value_1_Ucount, _koState);
        sendText(KoROB_CHStateText, stateText(_state), _koStateText);
        sendBool(KoROB_CHCleaning, isCleaning(_state), DPT_State, _koCleaning);
        sendBool(KoROB_CHCharging, _state == 8, DPT_State, _koCharging);
    }
    if (_battery >= 0) sendInt(KoROB_CHBattery, _battery, DPT_Scaling, _koBattery);
    const int level = fanSpeedLevel(_fanPower);
    if (level >= 0) sendInt(KoROB_CHFanSpeedStatus, level, DPT_Value_1_Ucount, _koFanSpeed);
    if (_errorCode >= 0)
    {
        sendBool(KoROB_CHError, _errorCode != 0, DPT_Alarm, _koError);
        sendInt(KoROB_CHErrorCode, _errorCode, DPT_Value_1_Ucount, _koErrorCode);
        sendText(KoROB_CHErrorText, errorText(_errorCode), _koErrorText);
    }

    // Last cleaning: area in mm², time in seconds.
    if (!s["clean_area"].isNull()) sendFloat(KoROB_CHCleanArea, s["clean_area"].as<float>() / 1000000.0f, DPT_Value_Area, _koCleanArea, 0.1f);
    if (!s["clean_time"].isNull()) sendInt(KoROB_CHCleanTime, s["clean_time"].as<int32_t>() / 60, DPT_TimePeriodMin, _koCleanTime);
}

void RoborockChannel::applyConsumables(const char *json)
{
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    JsonObjectConst c = doc["result"][0];
    if (c.isNull()) return;

    GroupObject *kos[4] = {&KoROB_CHMainBrush, &KoROB_CHSideBrush, &KoROB_CHFilter, &KoROB_CHSensors};
    for (uint8_t i = 0; i < 4; i++)
    {
        if (c[CONSUMABLE_FIELD[i]].isNull()) continue;
        const uint32_t used = c[CONSUMABLE_FIELD[i]].as<uint32_t>();
        const int remaining = used >= LIFETIME_S[i] ? 0 : (int)(100 - (uint64_t)used * 100 / LIFETIME_S[i]);
        _consumables[i] = remaining;
        sendInt(*kos[i], remaining, DPT_Scaling, _koConsumable[i]);
    }
}

void RoborockChannel::resetSendCache()
{
    _koReachable = {};
    _koFanSpeed = {};
    _koState = {};
    _koCleaning = {};
    _koCharging = {};
    _koBattery = {};
    _koError = {};
    _koErrorCode = {};
    _koCleanTime = {};
    for (auto &c : _koConsumable) c = {};
    _koCleanArea = {};
    _koStateText = {};
    _koErrorText = {};
    // Reachability is not part of a poll result, so it is repeated here directly.
    if (_lastResult != MiioClient::Pending) sendBool(KoROB_CHReachable, _reachable, DPT_State, _koReachable);
}

void RoborockChannel::enqueue(const char *method, const char *params)
{
    if (_queueCount >= QUEUE_SIZE)
    {
        logInfoP("Befehlspuffer voll, %s verworfen", method);
        return;
    }
    Command &cmd = _queue[(_queueHead + _queueCount) % QUEUE_SIZE];
    strncpy(cmd.method, method, sizeof(cmd.method) - 1);
    cmd.method[sizeof(cmd.method) - 1] = 0;
    strncpy(cmd.params, params, sizeof(cmd.params) - 1);
    cmd.params[sizeof(cmd.params) - 1] = 0;
    _queueCount++;
}

void RoborockChannel::processInputKo(GroupObject &ko)
{
    const int index = ROB_KoCalcIndex(ko.asap());
    if (index < 0 || !_client.configured()) return;

    switch (index)
    {
        case ROB_KoCHStart:
            if (ko.value(DPT_Switch)) enqueue("app_start");
            break;
        case ROB_KoCHPause:
            if (ko.value(DPT_Switch)) enqueue("app_pause");
            break;
        case ROB_KoCHStop:
            if (ko.value(DPT_Switch)) enqueue("app_stop");
            break;
        case ROB_KoCHDock:
            if (ko.value(DPT_Switch)) enqueue("app_charge");
            break;
        case ROB_KoCHLocate:
            if (ko.value(DPT_Switch)) enqueue("find_me", "[\"\"]");
            break;
        case ROB_KoCHFanSpeed:
        {
            const uint8_t level = ko.value(DPT_Value_1_Ucount);
            if (level > 3) break;
            char params[8];
            snprintf(params, sizeof(params), "[%u]", (unsigned)(101 + level));
            enqueue("set_custom_mode", params);
            break;
        }
        case ROB_KoCHConsumableReset:
        {
            const uint8_t which = ko.value(DPT_Value_1_Ucount);
            if (which < 1 || which > 4) break;
            char params[32];
            snprintf(params, sizeof(params), "[\"%s\"]", CONSUMABLE_FIELD[which - 1]);
            enqueue("reset_consumable", params);
            break;
        }
    }
}

bool RoborockChannel::consoleCall(const char *method, const char *params)
{
    if (!_client.configured() || _console.method[0] || _pending == PENDING_CONSOLE) return false;
    strncpy(_console.params, params, sizeof(_console.params) - 1);
    _console.params[sizeof(_console.params) - 1] = 0;
    strncpy(_console.method, method, sizeof(_console.method) - 1);
    _console.method[sizeof(_console.method) - 1] = 0;
    return true;
}

void RoborockChannel::printStatus()
{
    if (!_client.configured())
    {
        logInfoP("nicht konfiguriert (IP/Token ungültig)");
        return;
    }
    logInfoP("%s, Geräte-ID %lu, Handshake %s, letzter Aufruf: %s", _reachable ? "erreichbar" : "nicht erreichbar",
             (unsigned long)_client.deviceId(), _client.handshakeValid() ? "ok" : "neu", MiioClient::resultText(_lastResult));
    logInfoP("Zustand %d (%s), Akku %d %%, Saugstufe %d, Fehler %d (%s)", _state, stateText(_state), _battery, _fanPower, _errorCode,
             errorText(_errorCode));
    logInfoP("Restlaufzeit Hauptbürste %d %%, Seitenbürste %d %%, Filter %d %%, Sensoren %d %%", _consumables[0], _consumables[1],
             _consumables[2], _consumables[3]);
}

#endif // OPENKNX_ROBOROCK
