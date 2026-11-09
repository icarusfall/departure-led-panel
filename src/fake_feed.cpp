#include "fake_feed.h"
#include <Arduino.h>
#include <string.h>

// One simulated "minute" passes every few seconds.
static const uint32_t SIM_MINUTE_MS = 4000;

// Illustrative Streatham-area services; not real timetable data.
static const struct { const char *route, *dest; } POOL[] = {
    {"250", "Brixton"},
    {"109", "Croydon Town Centre"},
    {"133", "Liverpool Street"},
    {"159", "Marble Arch"},
    {"57", "Clapham Park"},
    {"118", "Morden"},
    {"60", "Old Coulsdon"},
    {"N133", "Liverpool Street"},
};
static const int POOL_N = sizeof(POOL) / sizeof(POOL[0]);

static Departure deps[FAKE_MAX];
static int depCount = 0;
static uint32_t lastTick = 0;

static void setFromPool(Departure &d, int minutes) {
  const auto &p = POOL[random(POOL_N)];
  strlcpy(d.route, p.route, sizeof(d.route));
  strlcpy(d.destination, p.dest, sizeof(d.destination));
  d.minutes = minutes;
}

static void sortByMinutes() {
  for (int i = 1; i < depCount; i++) {
    Departure key = deps[i];
    int j = i - 1;
    while (j >= 0 && deps[j].minutes > key.minutes) {
      deps[j + 1] = deps[j];
      j--;
    }
    deps[j + 1] = key;
  }
}

void fakeFeedBegin() {
  randomSeed(esp_random());
  depCount = FAKE_MAX;
  for (int i = 0; i < depCount; i++) setFromPool(deps[i], i * 3 + random(3));
  sortByMinutes();
  lastTick = millis();
}

bool fakeFeedUpdate(uint32_t nowMs) {
  if (nowMs - lastTick < SIM_MINUTE_MS) return false;
  lastTick = nowMs;
  for (int i = 0; i < depCount; i++) {
    // Departed buses are replaced by a new arrival further out.
    if (--deps[i].minutes < 0) setFromPool(deps[i], 12 + random(14));
  }
  sortByMinutes();
  return true;
}

const Departure *fakeFeedList() { return deps; }
int fakeFeedCount() { return depCount; }
