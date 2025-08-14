#include "tieralloc.h"
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <algorithm>

// Internal capacity model and helpers
extern "C" unsigned long long __ta_bytes_current(int tier); // forward from stats
extern "C" void __ta_set_capacity_soft(const unsigned long long soft[3]);
extern "C" void __ta_set_capacity_hard(const unsigned long long hard[3]);
extern "C" void __ta_inc_capacity_violation(int tier);

namespace {

enum class HardCapAction { Fail, RouteSlow };

struct CapConfig {
  unsigned long long soft[3]{0,0,0}; 
  unsigned long long hard[3]{0,0,0};
  HardCapAction on_hardcap{HardCapAction::RouteSlow};
};

CapConfig g_cap;

// 1k, 64m, 8g style parsing
static inline unsigned long long parse_size(const char* s, unsigned long long defv) {
  if (!s || !*s) return defv;
  char* end=nullptr;
  unsigned long long val = std::strtoull(s, &end, 10);
  if (!end || *end == '\0') return val;
  char suf = std::tolower(*end);
  if (suf=='k') return val<<10;
  if (suf=='m') return val<<20;
  if (suf=='g') return val<<30;
  return val;
}

static inline void load_caps_from_env() {
  // Soft
  g_cap.soft[TA_TIER_FAST]   = parse_size(std::getenv("TA_FAST_SOFT"),   0);
  g_cap.soft[TA_TIER_NORMAL] = parse_size(std::getenv("TA_NORMAL_SOFT"), 0);
  g_cap.soft[TA_TIER_SLOW]   = parse_size(std::getenv("TA_SLOW_SOFT"),   0);
  // Hard
  g_cap.hard[TA_TIER_FAST]   = parse_size(std::getenv("TA_FAST_HARD"),   0);
  g_cap.hard[TA_TIER_NORMAL] = parse_size(std::getenv("TA_NORMAL_HARD"), 0);
  g_cap.hard[TA_TIER_SLOW]   = parse_size(std::getenv("TA_SLOW_HARD"),   0);

  const char* act = std::getenv("TA_ON_HARDCAP");
  if (act && std::strcmp(act, "fail")==0) g_cap.on_hardcap = HardCapAction::Fail;

  __ta_set_capacity_soft(g_cap.soft);
  __ta_set_capacity_hard(g_cap.hard);
}

static inline ta_tier_t hint_to_tier(ta_hint_t hint) {
  switch (hint) {
    case TA_HINT_HOT:
    case TA_HINT_PIN_FAST:
    case TA_HINT_PREFER_FAST: return TA_TIER_FAST;
    case TA_HINT_COLD:        return TA_TIER_SLOW;
    case TA_HINT_WARM:        return TA_TIER_NORMAL;
    case TA_HINT_DEFAULT:
    default:                  return TA_TIER_NORMAL;
  }
}

// Try to select a tier that respects soft/hard caps
// Returns final chosen tier (may be different than hinted)
static inline ta_tier_t apply_caps(unsigned long long bytes, ta_tier_t want) {
  auto fits_soft = [&](int t){
    if (g_cap.soft[t] == 0) return true;
    unsigned long long cur = __ta_bytes_current(t);
    return (cur + bytes) <= g_cap.soft[t];
  };
  auto fits_hard = [&](int t){
    if (g_cap.hard[t] == 0) return true;
    unsigned long long cur = __ta_bytes_current(t);
    return (cur + bytes) <= g_cap.hard[t];
  };

  if (fits_hard(want) && fits_soft(want)) return want;

  // Downshift order: FAST->NORMAL->SLOW; NORMAL->FAST->SLOW; SLOW->NORMAL->FAST
  const int order[3][3] = {
    {TA_TIER_FAST, TA_TIER_NORMAL, TA_TIER_SLOW},
    {TA_TIER_NORMAL, TA_TIER_FAST, TA_TIER_SLOW},
    {TA_TIER_SLOW, TA_TIER_NORMAL, TA_TIER_FAST}
  };

  for (int i=0;i<3;i++) {
    int t = order[want][i];
    if (fits_hard(t) && fits_soft(t)) {
      if (t != want) __ta_inc_capacity_violation(want);
      return (ta_tier_t)t;
    }
  }
 
  for (int i=0;i<3;i++) {
    int t = order[want][i];
    if (fits_hard(t)) {
      if (t != want) __ta_inc_capacity_violation(want);
      return (ta_tier_t)t;
    }
  }

  if (g_cap.on_hardcap == HardCapAction::RouteSlow)
    return TA_TIER_SLOW;
  return want;
}

} 

extern "C" ta_tier_t __ta_pick_tier_from_hint(ta_hint_t hint) {
  return hint_to_tier(hint);
}

// Choose final tier considering hint + caps
extern "C" ta_tier_t __ta_policy_pick_tier(unsigned long long bytes, ta_hint_t hint) {
  static bool inited = false;
  if (!inited) { load_caps_from_env(); inited = true; }
  ta_tier_t want = hint_to_tier(hint);
  return apply_caps(bytes, want);
}

