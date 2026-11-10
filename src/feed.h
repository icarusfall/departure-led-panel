// Live departures: buses direct from the TfL API, trains from our Railway
// proxy (which wraps National Rail Darwin). One background task does the
// fetching, so only one TLS connection is open at a time and the display
// keeps animating.
#pragma once
#include <stddef.h>
#include "board.h"

constexpr int FEED_MAX = 8;

enum FeedKind { FEED_NONE, FEED_BUS, FEED_TRAIN };

enum FeedStatus {
  FEED_NOT_CONFIGURED,  // missing stop/station, key, token or proxy URL
  FEED_LOADING,         // waiting for the first response
  FEED_OK,
  FEED_ERROR,           // no usable data; see error text
};

struct FeedRequest {
  FeedKind kind = FEED_NONE;
  char where[16] = "";   // bus: NaPTAN ID; train: station CRS, e.g. "SRC"
  char dir[8] = "";      // train only: "north" or "south"
  char secret[64] = "";  // bus: TfL app key; train: proxy token
  char base[96] = "";    // train only: proxy base URL
};

void feedBegin();

// Selects what to poll. Call every frame; a change triggers an immediate
// fetch, and FEED_NONE pauses polling.
void feedSet(const FeedRequest &req);

// Copies current departures (minutes counted down since the last fetch)
// into out[FEED_MAX]. err receives a short reason on FEED_ERROR.
FeedStatus feedSnapshot(Departure *out, int &count, char *err, size_t errLen);
