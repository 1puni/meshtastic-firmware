# T-Deck as a boat GPS

A small patch to [Meshtastic firmware](https://github.com/meshtastic/firmware)
that turns a LilyGO T-Deck Plus into a Wi-Fi GPS for a boat. The GNSS
receiver's own NMEA sentences go out as UDP broadcast on port 10110, the
conventional NMEA-0183-over-UDP port, at 3 Hz. A chartplotter, OpenCPN or an
autopilot on the same network can listen without pairing anything.

It was built to steer [gps-only-pilot](https://github.com/1puni/gps-only-pilot),
the experimental drill-motor autopilot on 1puni's boat. That autopilot steers on
course over ground, and a phone reporting every 5–15 seconds is not enough to
steer on. The T-Deck's u-blox M10 is.

**Experimental. Not upstream Meshtastic, not a marine navigation product.**
Keep a proper lookout and a way to take the helm by hand.

## What changed

The patch is one commit on `nmea-udp-broadcast`, based on Meshtastic
`v2.7.19.bb3d6d5`:

- `src/gps/NMEABroadcast.{h,cpp}`: collects complete sentences as bytes arrive
  from the GNSS UART and sends each one as a subnet-directed UDP broadcast
  (for example `192.168.1.255:10110`) while Wi-Fi is connected.
- `src/gps/GPS.cpp`: feeds every received byte to the broadcaster, and puts the
  M10 into full-power 3 Hz mode when the broadcast is enabled.
- `src/gps/ubx.h`: the UBX configuration messages for that mode.

There is no new setting. The existing `serial.mode = NMEA` switch, meaning "emit
NMEA from the GPS", turns it on.

**Why forward raw sentences?** Meshtastic's position message stores speed as a
whole number of km/h, about 0.54 knots of resolution. That is too coarse to steer
on. `$GNRMC` carries speed and course to two decimals. Forwarding raw sentences
also means any other NMEA source can feed the same port.

## Verified on hardware

On a T-Deck Plus connected to the boat's Wi-Fi, a Raspberry Pi counted 121
datagrams in 20 s (`$GNRMC` + `$GNGGA`), with sentence timestamps 0.33 s apart.
The autopilot switched to the T-Deck as its GPS source within a second of
starting.

This build has only been tested on the T-Deck Plus (`t-deck-tft`). The code
compiles for any Wi-Fi-capable ESP32 target, but the 3 Hz setup is specific to
the u-blox M10.

## Use it

1. Build and flash (below), or flash the app image from this fork's
   [Releases](https://github.com/1puni/meshtastic-firmware/releases).
2. Join the T-Deck to the boat's Wi-Fi as you normally would in Meshtastic.
3. Turn on the broadcast:

   ```sh
   meshtastic --set serial.mode NMEA
   meshtastic --set position.gps_update_interval 1   # keep the receiver awake
   ```

4. Listen from any machine on the same network:

   ```sh
   python3 -c "
   import socket
   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
   s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
   s.bind(('', 10110))
   while True: print(s.recvfrom(2048)[0].decode('latin-1').strip())
   "
   ```

   In OpenCPN, add a UDP network connection on port 10110.

## Known limits

- **Bluetooth goes off.** On ESP32, Meshtastic's Wi-Fi mode disables Bluetooth,
  so the phone app has to reach the node another way.
- **Turning it off does not put the receiver back to 1 Hz.** The 3 Hz
  full-power mode is also written to the receiver's battery-backed settings. It
  stays after you switch `serial.mode` back to `DEFAULT`, until those settings
  are lost after a long power-off. An attempt to restore 1 Hz hung the firmware
  and is not included.
- **Course is blank at a standstill.** The receiver leaves course over ground
  empty when the boat is not moving. Treat it as missing, not as 0°, which
  would read as due north.
- The broadcast is plain UDP on the local network, with no authentication. Use
  it on a boat network you control.

## Build

```sh
uv tool install --with pip platformio   # PlatformIO needs pip in its venv
pio run -e t-deck-tft
```

To keep node identity, keys and Wi-Fi settings, flash only the app image at
`0x10000`, not the factory image:

```sh
esptool.py --chip esp32s3 write_flash 0x10000 .pio/build/t-deck-tft/firmware-t-deck-tft-*.bin
```

Tip: a T-Deck with its screen asleep does not answer on USB, and looks exactly
like dead firmware. Wake the screen first.

## License

GPL-3.0, like upstream Meshtastic. The upstream README is kept as
[README.upstream.md](README.upstream.md).
