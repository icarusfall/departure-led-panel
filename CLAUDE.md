# Home Departure Board — Project Brief

A DIY live-departure LED sign for my home in Streatham, South London, inspired by rakau (rakaulondon.co.uk): live bus and train departures on an amber dot-matrix-style display, with voice control from a Google speaker. Don't copy rakau's name or branding.

I'm a confident hobbyist developer. I'll do the physical testing; tell me exactly what to plug in, press, or look at.

## Hardware (all in hand)

- **Controller:** Adafruit Matrix Portal S3 (ESP32-S3, 8MB flash, 2MB PSRAM, Wi-Fi). Bought on eBay from a UK seller, so possibly not running Adafruit's factory firmware.
- **Panels:** 3× Waveshare RGB-Matrix-P2.5-64x32. HUB75, 1/16 scan, 160×80mm each, up to 12W each at full white.
  - The Matrix Portal plugs directly onto panel 1's **input** HUB75 header, overhanging the panel edge. I initially used the output header by mistake; the input one is now confirmed.
  - Panels chain OUT→IN with the 16-pin ribbons supplied.
  - The final layout is undecided: 2 panels (128×32) or 3 panels (192×32). Keep the chain length configurable.
- **Power:** Mean Well GST40A05-P1J, 5V regulated (RS lists 5A, Mean Well's datasheet up to 6A), with a 2.1mm centre-positive plug. It feeds a 2.1mm female DC socket with screw terminals, which connects to the panel power leads.
- **Cable:** a USB-A to USB-C data cable for programming.

### Power rules (important)
- The Matrix Portal's +5V/GND screw posts are **outputs only**. USB power passes straight through them. Never feed external 5V into them while USB is connected.
- The panels are powered directly from the Mean Well. The Matrix Portal is powered over USB, and ground is shared through the HUB75 ribbon.
- Cap brightness in firmware from the start. Three panels at full white could exceed the supply's rating, and full brightness is unbearable indoors anyway.

## Current state
- **Milestones 1–4 are done.** Three panels are chained (192×32) and running the departure-board layout with live TfL buses for Conyers Road (north and south). Wi-Fi setup portal, settings page at `http://departures.local`, six modes (including the secret one) and a night schedule all work.
- **Hardware findings:** the Waveshare panels have green and blue swapped (fixed in the pin order in `src/display.cpp`). The panels are mounted upside down, so `DISPLAY_ROTATION=2`. Settings are in `platformio.ini`.
- **Milestone 5 (trains) is done.** Trains come through a Railway proxy (`proxy/`, Railway project `departure-board-proxy` in the personal workspace, at `https://train-proxy-production.up.railway.app`), using National Rail Darwin via the Rail Data Marketplace. Buses still come direct from TfL. Northbound means calling at central London, Balham or Clapham Junction within 8 stops. Railway chunks responses, so the proxy sets Content-Length and the board reads proxy replies whole.
- **Secrets:** the TfL key, stop IDs and proxy token live only on the board (NVS). `RDM_KEY` and `PROXY_TOKEN` live only in Railway variables.

## Toolchain
- PlatformIO with the Arduino framework. Board: Adafruit MatrixPortal ESP32-S3.
- Display library: Adafruit Protomatter (Adafruit's supported library for this board). **Take the HUB75 pin definitions from Protomatter's own MatrixPortal S3 example. Don't guess them.**
- Known quirk: ESP32-S3 boards may need a manual press of the reset button after upload. If upload fails, enter ROM bootloader mode by holding BOOT and pressing RESET.
- If Waveshare panels show swapped colours or garbled rows, check the RGB channel order and scan settings.

## Build order (each milestone ends with something I can see)

1. **Hello panel.** Test pattern, then a static amber line such as "250 Brixton 3 min" across the chained panels. Low brightness.
2. **Layout and fonts.** A departure-board look: 2–4 rows of route, destination and minutes, plus an optional scrolling message line. Works for both 128×32 and 192×32.
3. **Wi-Fi and config.** Wi-Fi provisioning via a captive portal on first boot. A small on-device web page for stops, display colour, brightness, scrolling message and a night dimming/off schedule. Store settings in NVS (Preferences). No hard-coded secrets in the repo.
4. **Buses.** TfL Unified API, `StopPoint/{naptanId}/Arrivals` with an app key, sorted by expected arrival. I'll supply the stop IDs.
5. **Trains (Streatham Common and/or nearby stations).** Choose between Realtime Trains API and National Rail Darwin. Darwin is SOAP, which is awkward on the board, so consider a small proxy on **Railway** (I already use it). It would merge buses and trains into one compact JSON endpoint the board polls. Discuss the trade-off with me before building it: easier iteration and less memory pressure on the board, versus a cloud dependency.
6. **Robustness.** Reconnect on Wi-Fi loss, handle API errors and stale data gracefully, add a watchdog, and possibly OTA updates.
7. **Voice control via Matter (last).** Expose display modes (e.g. Buses, Trains, Football) as Matter on/off endpoints using the Arduino-ESP32 Matter library, made mutually exclusive in firmware. Pairing with Google Home needs a test vendor ID and a project in the Google Home Developer Console; walk me through that. Matter, Wi-Fi, display buffers and JSON parsing all compete for memory, so prototype Matter on the board early in this milestone before integrating.
8. **Stretch goals.** Football results, weather, and Santander Cycles dock availability (also on the TfL API).

## Later (not now)
- A plywood case with a smoked acrylic front, laser-cut or CNC'd. Dimensions are set by the final panel count.
- Single-supply power for the finished unit, avoiding the back-powering issue above.
