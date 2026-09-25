#pragma once

#include "configuration.h"

/*
Mirrors the GNSS receiver's own NMEA sentences onto the LAN as UDP broadcasts,
so a chartplotter/autopilot on the same network gets a nav-rate feed.

We forward the receiver's bytes verbatim rather than re-emitting from
meshtastic_Position, because Position::ground_speed is a uint32 in km/h -- about
0.54 kn of resolution, which is too coarse to steer on. $GNRMC carries SOG and
COG to two decimals.

Gated on moduleConfig.serial.mode == NMEA, which already means "emit NMEA from
the GPS" and is exposed in the apps and CLI today. Note it is the *mode* that
matters, not serial.enabled -- you can broadcast without also driving a UART.
*/

#if HAS_WIFI && !defined(MESHTASTIC_EXCLUDE_WIFI) && !defined(ARCH_PORTDUINO)
#define HAS_NMEA_UDP_BROADCAST 1

// Feed one byte as it is read from the GNSS UART. Complete sentences are
// broadcast; anything else is dropped.
void nmeaBroadcastFeed(char c);

// True when the config asks for NMEA output. Also used to decide whether the
// receiver should be put in continuous, higher-rate mode.
bool nmeaBroadcastEnabled();

#else
#define HAS_NMEA_UDP_BROADCAST 0
static inline void nmeaBroadcastFeed(char) {}
static inline bool nmeaBroadcastEnabled()
{
    return false;
}
#endif
