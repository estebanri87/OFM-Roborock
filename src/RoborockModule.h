#pragma once
#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

#include "ROBChannelOwnerModule.h"
#include "RoborockKo.h"

// Roborock vacuum robots (classic miIO API, e.g. S5) on the KNX bus, locally over UDP
// without cloud. One channel per robot.
class RoborockModule : public ROBChannelOwnerModule
{
  public:
    RoborockModule();

    const std::string name() override;
    const std::string version() override;

    OpenKNX::Channel *createChannel(uint8_t _channelIndex) override;

    void showHelp() override;
    bool processCommand(const std::string command, bool debugKo) override;

    // Short status text for the module object "Diagnose" (max. 14 characters).
    void setDiag(const char *text);

  private:
    Roborock::LastText _lastDiag;
};

extern RoborockModule openknxRoborockModule;

#endif // OPENKNX_ROBOROCK
