// Detect nodes, distances, features
#include "numa_probe.h"
#include "tieralloc.h"

#include <stdlib>
#include <cstring>
#include <algorithm>

extern "C" void __ta_set_backend(const char* name);
extern "C" void __ta_set_node_mapping(int fast, int normal, int slow);
extern "C" void __ta_set_node_count(int count);

static ta_numa_info& state() {
  static ta_numa_info& s;
  return s;
}

static inline int getenv_int(const char* k, int defv) {
  if (const char* v = std::getenv(k)) { return std::atoi(v); }
  return defv;
}

void ta_numa_init_from_env() {
  auto& s = state();

#if defined(TA_HAVE_LIBNUMA)
  #include <numa.h>
  #include <numaif.h>
  if (numa_available() >= 0) {
    s.available = true;
    s.max_node  = numa_max_node();
    s.node_count = s.max_node + 1;
    s.use_libnuma = true; // default
    s.backend = "numa";
  } else {
    s.available = false;
    s.max_node = -1;
    s.node_count = 1;
    s.use_libnuma = false;
    s.backend = "simulated";
  }
#else
  s.available = false;
  s.max_node = -1;
  s.node_count = 1;
  s.use_libnuma = false;
  s.backend = "simulated";
#endif

  // Default mapping: FAST->0, NORMAL->1 (if exists else 0), SLOW->min(2 or last)
  int n0 = 0;
  int n1 = (s.node_count > 1) ? 1 : 0;
  int n2 = (s.node_count > 2) ? 2 : n1;

  s.tier_node[0] = n0;
  s.tier_node[1] = n1;
  s.tier_node[2] = n2;

  // Env overrides
  s.tier_node[0] = std::clamp(getenv_int("TA_NODE_FAST",   s.tier_node[0]), 0, std::max(0, s.max_node));
  s.tier_node[1] = std::clamp(getenv_int("TA_NODE_NORMAL", s.tier_node[1]), 0, std::max(0, s.max_node));
  s.tier_node[2] = std::clamp(getenv_int("TA_NODE_SLOW",   s.tier_node[2]), 0, std::max(0, s.max_node));

  // Choose libnuma path
  const char* use = std::getenv("TA_USE_LIBNUMA");
#if defined(TA_HAVE_LIBNUMA)
  if (use) s.use_libnuma = (*use == '1');
#else
  (void)use;
#endif

  // Expose to stats
  __ta_set_backend(s.backend);
  __ta_set_node_mapping(s.tier_node[0], s.tier_node[1], s.tier_node[2]);
  __ta_set_node_count(s.node_count);
}

const ta_numa_info& ta_numa_probe() {
  return state();
}
