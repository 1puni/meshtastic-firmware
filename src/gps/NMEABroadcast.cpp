#include "NMEABroadcast.h"

#if HAS_NMEA_UDP_BROADCAST

#include "NodeDB.h" // for moduleConfig
#include "mesh/wifi/WiFiAPClient.h"
#include <AsyncUDP.h>
#include <WiFi.h>

#ifndef NMEA_UDP_PORT
#define NMEA_UDP_PORT 10110 // the conventional NMEA-0183-over-UDP port
#endif

// NMEA-0183 caps a sentence at 82 chars including delimiters; allow headroom for
// receivers that overrun it, and drop anything longer as malformed.
#define NMEA_LINE_MAX 120

static char lineBuf[NMEA_LINE_MAX];
static size_t lineLen = 0;
static bool overrun = false;
static AsyncUDP nmeaUdp;

bool nmeaBroadcastEnabled()
{
    return moduleConfig.serial.mode == meshtastic_ModuleConfig_SerialConfig_Serial_Mode_NMEA;
}

// Subnet-directed broadcast (e.g. 192.168.14.255) rather than 255.255.255.255:
// limited broadcasts get dropped by some stacks and never leave the host.
static IPAddress broadcastAddress()
{
    uint32_t ip = (uint32_t)WiFi.localIP();
    uint32_t mask = (uint32_t)WiFi.subnetMask();
    return IPAddress(ip | ~mask);
}

static void sendLine()
{
    if (lineLen == 0)
        return;

    // Only forward real sentences. The M10 also emits UBX binary on this UART
    // during configuration, which must not be relayed to a chartplotter.
    if (lineBuf[0] != '$' && lineBuf[0] != '!')
        return;

    if (!isWifiAvailable() || WiFi.status() != WL_CONNECTED)
        return;

    // Terminate as the wire format expects; receivers split on CRLF.
    if (lineLen + 2 >= NMEA_LINE_MAX)
        return;
    lineBuf[lineLen] = '\r';
    lineBuf[lineLen + 1] = '\n';

    nmeaUdp.writeTo((const uint8_t *)lineBuf, lineLen + 2, broadcastAddress(), NMEA_UDP_PORT);
}

void nmeaBroadcastFeed(char c)
{
    if (!nmeaBroadcastEnabled()) {
        lineLen = 0;
        overrun = false;
        return;
    }

    if (c == '\r' || c == '\n') {
        if (!overrun)
            sendLine();
        lineLen = 0;
        overrun = false;
        return;
    }

    // A sentence starts at '$'; resync on it so a mid-stream join or a dropped
    // byte costs one sentence rather than corrupting every following one.
    if (c == '$' || c == '!') {
        lineLen = 0;
        overrun = false;
    }

    if (lineLen >= NMEA_LINE_MAX - 3) {
        overrun = true;
        lineLen = 0;
        return;
    }

    lineBuf[lineLen++] = c;
}

#endif // HAS_NMEA_UDP_BROADCAST
