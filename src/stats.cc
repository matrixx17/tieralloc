#include "tieralloc.h"
#include <atomic>
#include <cstring>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>

namespace {

// Per-tier counters
struct Counters {
  std::atomic<unsigned long long> alloc_calls[3]{};
  std::atomic<unsigned long long> free_calls[3]{};
  std::atomic<unsigned long long> bytes_current[3]{};
  std::atomic<unsigned long long> bytes_total_alloc[3]{};
  std::atomic<unsigned long long> bytes_total_freed[3]{};
  std::atomic<unsigned long long> simulated_wait_ns[3]{};
  std::atomic<unsigned long long> capacity_violations[3]{};

  // Config snapshot (plain values)
  unsigned long long capacity_soft[3]{0,0,0};
  unsigned long long capacity_hard[3]{0,0,0};

  // Migration counters
  std::atomic<unsigned long long> mig_attempted{0};
  std::atomic<unsigned long long> mig_moved_pages{0};
  std::atomic<unsigned long long> mig_failed_pages{0};

  // Backend & node topology
  std::string backend{"simulated"};
  int nodes_map[3]{0,0,0};
  int node_count{1};

  // Bytes per node (size set once via __ta_set_node_count)
  std::unique_ptr<std::atomic<unsigned long long>[]> node_bytes;
};

Counters& S() { static Counters c; return c; }
std::mutex g_cfg_mtx;

} // namespace

// --- Public P0 snapshot remains the same for backward compatibility ---
extern "C" void ta_get_stats(ta_stats_snapshot_t* out) {
  if (!out) return;
  auto& s = S();
  for (int i=0;i<3;++i) {
    out->alloc_calls[i]        = s.alloc_calls[i].load(std::memory_order_relaxed);
    out->free_calls[i]         = s.free_calls[i].load(std::memory_order_relaxed);
    out->bytes_current[i]      = s.bytes_current[i].load(std::memory_order_relaxed);
    out->bytes_total_alloc[i]  = s.bytes_total_alloc[i].load(std::memory_order_relaxed);
    out->bytes_total_freed[i]  = s.bytes_total_freed[i].load(std::memory_order_relaxed);
    out->simulated_wait_ns[i]  = s.simulated_wait_ns[i].load(std::memory_order_relaxed);
  }
}

extern "C" int ta_stats_json(char* buf, unsigned long long n) {
  auto& s = S();
  // Take a coherent snapshot
  unsigned long long ac[3], fc[3], bc[3], bta[3], btf[3], w[3], cv[3], soft[3], hard[3];
  for (int i=0;i<3;++i) {
    ac[i] = s.alloc_calls[i].load(std::memory_order_relaxed);
    fc[i] = s.free_calls[i].load(std::memory_order_relaxed);
    bc[i] = s.bytes_current[i].load(std::memory_order_relaxed);
    bta[i]= s.bytes_total_alloc[i].load(std::memory_order_relaxed);
    btf[i]= s.bytes_total_freed[i].load(std::memory_order_relaxed);
    w[i]  = s.simulated_wait_ns[i].load(std::memory_order_relaxed);
    cv[i] = s.capacity_violations[i].load(std::memory_order_relaxed);
    soft[i]= s.capacity_soft[i];
    hard[i]= s.capacity_hard[i];
  }
  unsigned long long migA = s.mig_attempted.load(std::memory_order_relaxed);
  unsigned long long migM = s.mig_moved_pages.load(std::memory_order_relaxed);
  unsigned long long migF = s.mig_failed_pages.load(std::memory_order_relaxed);

  std::ostringstream oss;
  auto arr3 = [&](const char* k, const unsigned long long a[3]){
    oss << "\"" << k << "\":[ " << a[0] << ", " << a[1] << ", " << a[2] << " ],";
  };
  oss << "{";
  arr3("alloc_calls", ac);
  arr3("free_calls", fc);
  arr3("bytes_current", bc);
  arr3("bytes_total_alloc", bta);
  arr3("bytes_total_freed", btf);
  arr3("simulated_wait_ns", w);
  arr3("capacity_soft", soft);
  arr3("capacity_hard", hard);
  arr3("capacity_violations", cv);
  oss << "\"backend\":\"" << s.backend << "\",";
  oss << "\"nodes\":[" << s.nodes_map[0] << "," << s.nodes_map[1] << "," << s.nodes_map[2] << "],";
  oss << "\"node_count\":" << s.node_count << ",";
  // bytes_per_node
  oss << "\"bytes_per_node\":[";
  for (int i = 0; i < s.node_count; i++) {
    unsigned long long val = s.node_bytes
      ? s.node_bytes[i].load(std::memory_order_relaxed) : 0ull;
    oss << val << (i+1<s.node_count ? "," : "");
  }
  oss << "],";
  // migrations
  oss << "\"migrations\":{\"attempted\":" << migA
      << ",\"moved_pages\":" << migM
      << ",\"failed_pages\":" << migF << "}";

  std::string json = oss.str();
  if (!json.empty() && json.back()==',') json.back() = '}';
  else json.push_back('}');

  if (!buf || n==0) return (int)json.size();
  unsigned long long m = (unsigned long long)json.size();
  unsigned long long c = (m < n-1) ? m : (n-1);
  std::memcpy(buf, json.data(), c);
  buf[c] = '\0';
  return (int)m;
}

// --- Internal helpers used by allocator/policy/numa ---

extern "C" void __ta_add_alloc(ta_tier_t t, unsigned long long sz, long wait_ns) {
  auto& s = S();
  s.alloc_calls[(int)t]++;
  s.bytes_current[(int)t] += sz;
  s.bytes_total_alloc[(int)t] += sz;
  if (wait_ns > 0) s.simulated_wait_ns[(int)t] += (unsigned long long)wait_ns;
}

extern "C" void __ta_add_free(ta_tier_t t, unsigned long long sz) {
  auto& s = S();
  s.free_calls[(int)t]++;
  s.bytes_current[(int)t] -= sz;
  s.bytes_total_freed[(int)t] += sz;
}

// Per-tier current (used by policy to check caps)
extern "C" unsigned long long __ta_bytes_current(int tier) {
  return S().bytes_current[tier].load(std::memory_order_relaxed);
}

// Set capacity snapshots for stats (called from policy init)
extern "C" void __ta_set_capacity_soft(const unsigned long long soft[3]) {
  auto& s = S();
  for (int i=0;i<3;i++) s.capacity_soft[i] = soft[i];
}
extern "C" void __ta_set_capacity_hard(const unsigned long long hard[3]) {
  auto& s = S();
  for (int i=0;i<3;i++) s.capacity_hard[i] = hard[i];
}
extern "C" void __ta_inc_capacity_violation(int tier) {
  S().capacity_violations[tier]++;
}

// Backend + node mapping exposed to stats
extern "C" void __ta_set_backend(const char* name) {
  S().backend = name ? name : "simulated";
}
extern "C" void __ta_set_node_mapping(int fast, int normal, int slow) {
  S().nodes_map[0] = fast; S().nodes_map[1] = normal; S().nodes_map[2] = slow;
}
extern "C" void __ta_set_node_count(int count) {
  auto& s = S();
  s.node_count = std::max(1, count);
  s.node_bytes.reset(new std::atomic<unsigned long long>[s.node_count]);
  for (int i = 0; i < s.node_count; i++) {
    s.node_bytes[i].store(0ull, std::memory_order_relaxed);
  }
}

extern "C" void __ta_bytes_node_add(int node, long long delta) {
  auto& s = S();
  if (node < 0 || node >= s.node_count || !s.node_bytes) return;
  if (delta >= 0) {
    s.node_bytes[node].fetch_add((unsigned long long)delta, std::memory_order_relaxed);
  } else {
    unsigned long long d = (unsigned long long)(-delta);
    for (;;) {
      unsigned long long cur = s.node_bytes[node].load(std::memory_order_relaxed);
      unsigned long long next = (d > cur) ? 0ull : (cur - d);
      if (s.node_bytes[node].compare_exchange_weak(cur, next, std::memory_order_relaxed)) break;
    }
  }
}

// Migration counters
extern "C" void __ta_add_migration(unsigned long long attempted, unsigned long long moved_pages, unsigned long long failed_pages) {
  auto& s = S();
  s.mig_attempted += attempted;
  s.mig_moved_pages += moved_pages;
  s.mig_failed_pages += failed_pages;
}
