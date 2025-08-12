#include "tieralloc.h"

extern "C" ta_tier_t __ta_pick_tier_from_hint(ta_hint_t hint) {
    switch (hint) {
        case TA_HINT_HOT:         return TA_TIER_FAST;
        case TA_HINT_PIN_FAST:    return TA_TIER_FAST;
        case TA_HINT_PREFER_FAST: return TA_TIER_FAST; // fallback to NORMAL later if needed
        case TA_HINT_COLD:        return TA_TIER_SLOW;
        case TA_HINT_WARM:        return TA_TIER_NORMAL;
        case TA_HINT_DEFAULT:
        default:                  return TA_TIER_NORMAL;
    }
}

