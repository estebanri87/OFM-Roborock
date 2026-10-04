#pragma once
#include "RoborockConfig.h"
#ifdef OPENKNX_ROBOROCK

#include "OpenKNX.h"
#include <math.h>
#include <string>

namespace Roborock
{
    // Send-on-change for status objects (pattern of OFM-Vaillant/VaillantKo.h). The robot is
    // polled every few seconds; without this every poll would put every value on the bus.

    struct LastFloat
    {
        float value = NAN;
    };
    struct LastInt
    {
        int32_t value = INT32_MIN;
    };
    struct LastText
    {
        std::string value;
        bool sent = false;
    };

    inline void sendFloat(GroupObject &ko, float v, const Dpt &dpt, LastFloat &last, float epsilon = 0.01f)
    {
        if (isnan(v)) return;
        if (!isnan(last.value) && fabsf(v - last.value) < epsilon) return;
        last.value = v;
        ko.value(v, dpt);
    }

    inline void sendInt(GroupObject &ko, int32_t v, const Dpt &dpt, LastInt &last)
    {
        if (last.value == v) return;
        last.value = v;
        ko.value(v, dpt);
    }

    inline void sendBool(GroupObject &ko, bool v, const Dpt &dpt, LastInt &last)
    {
        if (last.value == (int32_t)v) return;
        last.value = v;
        ko.value(v, dpt);
    }

    // DPT 16.001 is ISO-8859-1 with 14 characters. The texts in the source are UTF-8, so
    // two-byte sequences (umlauts) are folded to one Latin-1 byte.
    inline std::string latin1(const char *utf8)
    {
        std::string out;
        for (const uint8_t *p = (const uint8_t *)(utf8 ? utf8 : ""); *p && out.size() < 14; p++)
        {
            if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
            {
                out += (char)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
                p++;
            }
            else if (p[0] < 0x80)
                out += (char)p[0];
        }
        return out;
    }

    inline void sendText(GroupObject &ko, const char *v, LastText &last)
    {
        const std::string text = latin1(v);
        if (last.sent && last.value == text) return;
        last.value = text;
        last.sent = true;
        ko.value(text.c_str(), DPT_String_8859_1);
    }
} // namespace Roborock

#endif // OPENKNX_ROBOROCK
