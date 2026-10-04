#pragma once

// OFM-Roborock is ESP32-only.
//
// The miIO protocol encrypts every packet with AES-128-CBC and signs it with MD5. On ESP32
// both come from mbedTLS (AES in hardware). The supported hardware of OAM-NetworkService is
// ESP32-based, so the module is not ported to RP2040.
//
// Every header and source file of this module is wrapped in OPENKNX_ROBOROCK, and the
// parent project guards its addModule() call with it. This header itself stays
// unguarded so that guard can be evaluated.

#if defined(ARDUINO_ARCH_ESP32) && (defined(KNX_IP_WIFI) || defined(KNX_IP_LAN))
#define OPENKNX_ROBOROCK 1
#endif
