#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

#include "RoborockModule.h"
#include "RoborockChannel.h"

using namespace Roborock;

namespace
{
    // "Aktiv" in the module list of OpenKNX/Common. Without that list the module is on.
    bool moduleEnabled()
    {
#ifdef ParamBASE_ModuleEnabled_ROB
        return ParamBASE_ModuleEnabled_ROB;
#else
        return true;
#endif
    }
} // namespace

RoborockModule::RoborockModule()
    : ROBChannelOwnerModule(ROB_ChannelCount)
{
}

const std::string RoborockModule::name()
{
    return "Roborock";
}

const std::string RoborockModule::version()
{
#ifdef MODULE_Roborock_Version
    return MODULE_Roborock_Version;
#else
    return "0.1.0";
#endif
}

OpenKNX::Channel *RoborockModule::createChannel(uint8_t _channelIndex)
{
    // Nur aktivierte, nicht suspendierte Kanäle anlegen (Kanalauswahl).
    if (!moduleEnabled() || ParamROB_CHType == RoborockChannel::TYPE_NONE || ParamROB_CHSuspended) return nullptr;
    return new RoborockChannel(_channelIndex, *this);
}

void RoborockModule::setDiag(const char *text)
{
    sendText(KoROB_ROBDiag, text, _lastDiag);
}

void RoborockModule::showHelp()
{
    openknx.console.printHelpLine("rob", "Roborock: Status aller Kanäle");
    openknx.console.printHelpLine("rob call <ch> <method> [json]", "Roborock: miIO-Befehl senden, z.B. 'rob call 1 get_status'");
}

bool RoborockModule::processCommand(const std::string command, bool debugKo)
{
    if (command.rfind("rob", 0) != 0) return false;

    if (command == "rob")
    {
        logInfoP("aktiv: %s, Kanäle: %u", moduleEnabled() ? "ja" : "nein", (unsigned)getNumberOfUsedChannels());
        for (uint8_t i = 0; i < getNumberOfChannels(); i++)
            if (auto *channel = static_cast<RoborockChannel *>(getChannel(i))) channel->printStatus();
        return true;
    }

    if (command.rfind("rob call ", 0) == 0)
    {
        unsigned ch = 0;
        char method[32] = {0};
        int consumed = 0;
        if (sscanf(command.c_str(), "rob call %u %31s %n", &ch, method, &consumed) < 2 || ch < 1 || ch > getNumberOfChannels())
        {
            logInfoP("Aufruf: rob call <Kanal> <Methode> [JSON-Parameter]");
            return true;
        }
        const char *params = consumed > 0 && command.c_str()[consumed] ? command.c_str() + consumed : "[]";
        auto *channel = static_cast<RoborockChannel *>(getChannel(ch - 1));
        if (channel == nullptr)
            logInfoP("Kanal %u nicht aktiv", ch);
        else
            logInfoP("%s %s: %s", method, params, channel->consoleCall(method, params) ? "eingereiht" : "nicht möglich");
        return true;
    }
    return false;
}

#endif // OPENKNX_ROBOROCK
