// Voice control: each display mode is a Matter on/off plug, so Google Home
// (or any Matter controller) can switch modes. Exactly one is on at a time.
// Uses Bluetooth commissioning the first time; BLE memory is released once
// paired.
#pragma once
#include <Arduino.h>
#include <stdint.h>

// Creates the endpoints and starts Matter, showing `mode` as the active one.
// Call after Wi-Fi is up.
void matterModesBegin(uint8_t mode);

// Call every loop.
void matterModesLoop();

// Returns a mode selected from Google Home since the last call, or -1.
int matterModesTakeRequest();

// Shows a mode chosen elsewhere (e.g. the settings page) in Google Home.
void matterModesShow(uint8_t mode);

bool matterModesCommissioned();
String matterModesPairingCode();
String matterModesQrUrl();

// Removes the board from Google Home and reopens pairing.
void matterModesUnpair();
