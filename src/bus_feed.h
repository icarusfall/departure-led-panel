// Live bus arrivals from the TfL API (StopPoint/{naptanId}/Arrivals).
// Fetching runs in a background task so the display keeps animating.
#pragma once
#include <stddef.h>
#include "board.h"

constexpr int BUS_MAX = 8;

enum BusStatus {
  BUS_NOT_CONFIGURED,  // no stop ID or app key set
  BUS_LOADING,         // waiting for the first response
  BUS_OK,
  BUS_ERROR,           // no usable data; see error text
};

void busFeedBegin();

// Selects the stop to poll. Call every frame; an empty stop pauses polling.
// A change triggers an immediate fetch.
void busFeedSetStop(const char *naptanId, const char *appKey);

// Copies the current departures (minutes counted down since the last
// fetch) into out[BUS_MAX]. err receives a short reason on BUS_ERROR.
BusStatus busFeedSnapshot(Departure *out, int &count, char *err,
                          size_t errLen);
