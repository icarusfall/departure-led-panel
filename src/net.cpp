#include "net.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include "display.h"

static const int DOWN_BUTTON = 7;           // Matrix Portal S3 DOWN button
static const int PORTAL_TIMEOUT_S = 300;    // then carry on offline
static const char *UK_TZ = "GMT0BST,M3.5.0/1,M10.5.0";

bool netBegin() {
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);

  WiFiManager wm;
  wm.setDebugOutput(true);
  if (digitalRead(DOWN_BUTTON) == LOW) {
    displayLines("Forgetting saved Wi-Fi");
    wm.resetSettings();
    delay(1500);
  }

  wm.setTitle("Departure board");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setAPCallback([](WiFiManager *) {
    Serial.println("Setup portal open: " SETUP_AP_NAME);
    displayLines("Wi-Fi setup", "On your phone, join Wi-Fi:", SETUP_AP_NAME);
  });

  displayLines("Connecting to Wi-Fi...");
  const bool ok = wm.autoConnect(SETUP_AP_NAME);
  WiFi.setAutoReconnect(true);

  // Time is needed for the night schedule; sync starts in the background.
  configTzTime(UK_TZ, "pool.ntp.org", "time.google.com");

  if (!ok) {
    Serial.println("Wi-Fi not connected; running offline");
    displayLines("No Wi-Fi", "Running offline");
    delay(2500);
    return false;
  }

  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  const String ip = WiFi.localIP().toString();
  Serial.printf("Connected. http://%s.local  http://%s\n", HOSTNAME,
                ip.c_str());
  displayLines("Settings page:", "departures.local", ip.c_str());
  delay(5000);
  return true;
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }

static const uint32_t RETRY_MS = 20000;
static const uint32_t GIVE_UP_MS = 10 * 60000;

void netLoop() {
  static bool wasUp = true;  // netBegin's result is reported separately
  static uint32_t downSince = 0, lastTry = 0;
  const uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasUp) {
      Serial.printf("Wi-Fi back after %lu s\n",
                    (unsigned long)((now - downSince) / 1000));
      MDNS.end();
      if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    }
    wasUp = true;
    return;
  }

  if (wasUp) {
    Serial.println("Wi-Fi lost");
    wasUp = false;
    downSince = lastTry = now;
  }
  if (now - lastTry > RETRY_MS) {
    lastTry = now;
    Serial.println("Wi-Fi: retrying");
    WiFi.reconnect();
  }
  if (now - downSince > GIVE_UP_MS) {
    Serial.println("Wi-Fi down for 10 min; restarting");
    delay(100);
    ESP.restart();
  }
}

bool netLocalMinutes(int &minutes) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // not synced yet
  struct tm t;
  localtime_r(&now, &t);
  minutes = t.tm_hour * 60 + t.tm_min;
  return true;
}
