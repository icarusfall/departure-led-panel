#include "net.h"
#include <ESPmDNS.h>
#include <mdns.h>
#include <string.h>
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
static const uint32_t NAME_CHECK_MS = 10000;

// Matter shares the mDNS service and replaces our hostname with its own
// (MAC-based) one. Rather than fight it, publish departures.local as an
// extra delegated name pointing at our IP, with the settings page service.
static void keepNameAdvertised() {
  static uint32_t lastCheck = 0, lastIp = 0;
  static bool delegated = false;
  if (millis() - lastCheck < NAME_CHECK_MS || WiFi.status() != WL_CONNECTED)
    return;
  lastCheck = millis();

  char primary[64] = "";
  mdns_hostname_get(primary);
  const uint32_t ip = (uint32_t)WiFi.localIP();
  if (!delegated && strcmp(primary, HOSTNAME) == 0) return;  // still ours

  mdns_ip_addr_t addr = {};
  addr.addr.type = ESP_IPADDR_TYPE_V4;
  addr.addr.u_addr.ip4.addr = ip;
  if (!delegated) {
    if (mdns_delegate_hostname_add(HOSTNAME, &addr) == ESP_OK) {
      mdns_service_add_for_host(nullptr, "_http", "_tcp", HOSTNAME, 80,
                                nullptr, 0);
      delegated = true;
      Serial.printf("mDNS: primary name is now '%s'; added %s.local\n",
                    primary, HOSTNAME);
    }
  } else if (ip != lastIp) {
    mdns_delegate_hostname_set_address(HOSTNAME, &addr);
    Serial.println("mDNS: updated departures.local address");
  }
  lastIp = ip;
}

void netLoop() {
  keepNameAdvertised();
  static bool wasUp = true;  // netBegin's result is reported separately
  static uint32_t downSince = 0, lastTry = 0;
  const uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasUp) {
      // mDNS re-announces by itself. Don't restart it: Matter shares the
      // same mDNS service and would lose its adverts.
      Serial.printf("Wi-Fi back after %lu s\n",
                    (unsigned long)((now - downSince) / 1000));
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
