#pragma once
#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

#include "MiioClient.h"
#include "OpenKNX.h"
#include "RoborockKo.h"

class RoborockModule;

// One channel = one robot, reached locally over miIO (IP + token).
class RoborockChannel : public OpenKNX::Channel
{
  public:
    // Values of ParamROB_CHType
    enum ChannelType : uint8_t
    {
        TYPE_NONE = 0,
        TYPE_MIIO = 1,
    };

    RoborockChannel(uint8_t index, RoborockModule &module);

    const std::string name() override;
    void setup() override;
    void loop() override;
    void processInputKo(GroupObject &ko) override;

    void printStatus();
    // Console: send any miIO command, the answer goes to the log.
    bool consoleCall(const char *method, const char *params);

  private:
    enum Pending : uint8_t
    {
        PENDING_NONE,
        PENDING_STATUS,
        PENDING_CONSUMABLE,
        PENDING_COMMAND,
        PENDING_CONSOLE,
    };

    struct Command
    {
        char method[32];
        char params[48];
    };
    static constexpr uint8_t QUEUE_SIZE = 4;

    RoborockModule &_module;
    MiioClient _client;
    Pending _pending = PENDING_NONE;

    uint16_t _pollSeconds = 0;
    uint8_t _consumableMinutes = 0;
    uint8_t _cyclicMinutes = 0;
    uint32_t _lastStatusMs = 0;
    uint32_t _lastConsumableMs = 0;
    uint32_t _lastCyclicMs = 0;
    bool _statusDue = true;
    bool _consumableDue = true;

    Command _queue[QUEUE_SIZE];
    uint8_t _queueHead = 0;
    uint8_t _queueCount = 0;
    Command _console = {}; // method[0] != 0: console call waiting

    uint8_t _failures = 0;
    bool _reachable = false;
    MiioClient::Result _lastResult = MiioClient::Pending;

    // Last decoded values, for the console.
    int _state = -1, _battery = -1, _fanPower = -1, _errorCode = -1;
    int _consumables[4] = {-1, -1, -1, -1};

    Roborock::LastInt _koReachable, _koFanSpeed, _koState, _koCleaning, _koCharging, _koBattery, _koError, _koErrorCode,
        _koCleanTime, _koConsumable[4];
    Roborock::LastFloat _koCleanArea;
    Roborock::LastText _koStateText, _koErrorText;

    void startNextCall();
    void handleFinished();
    void enqueue(const char *method, const char *params = "[]");
    void setReachable(bool reachable);
    void applyStatus(const char *json);
    void applyConsumables(const char *json);
    void resetSendCache();
};

#endif // OPENKNX_ROBOROCK
