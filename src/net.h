// Wi-Fi (with captive-portal provisioning), mDNS and network time.
#pragma once
#include <stdint.h>

#define HOSTNAME "departures"  // http://departures.local
#define SETUP_AP_NAME "Departures-Setup"

// Connects to saved Wi-Fi, or opens the setup portal if there is none or
// it can't connect. Hold the DOWN button during boot to forget saved Wi-Fi.
// Shows progress on the panels. Returns true if connected.
bool netBegin();

bool netConnected();

// Local (UK) time as minutes since midnight; false until NTP has synced.
bool netLocalMinutes(int &minutes);
