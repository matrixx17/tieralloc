#pragma once
#include <cstdint>

struct ta_numa_info {
  bool available{false};    // NUMA presence
  int max_node{-1};         // highest node id
  int tier_node[3]{0,0,0};  // mapping: FAST/NORMAL/SLOW; node id
  bool use_libnuma{false};  // chooose libnuma path over mbind
  int node_count{1};        // number of nodes (=max_node+1 if available)
  const char* backend{"simulated"};   // "numa" or "simulated"
};

// Returns reference to singleton populated at init time
// Immutable after init
const ta_numa_info& ta_numa_probe();

// Sets backend and node mapping
void ta_numa_init_from_env();
