#include "tieralloc.h"

#include <atomic>
#include <cstring>
#include <string>
#include <sstream>
#include <mutex>

namespace {
struct Counters {
    std::atomic<unsigned long long> alloc_calls[3]{};
    std::atomic<unsigned long long> free_calls[3]{};
    std::atomic<unsigned long long> bytes_current[3]{};
    std::atomic<unsigned long long> bytes_total_alloc[3]{};
    std::atomic<unsigned long long> bytes_total_freed[3]{};
    std::atomic<unsigned long long> simulated_wait_ns[3]{};
};

Counters g_ctr;
std::mutex g_cfg_mtx;

} 

extern "C" void ta_get_stats(ta_stats_snapshot_t* out) {
    if (!out) return;
    for (int i=0;i<3;++i) {
        out->alloc_calls[i]        = g_ctr.alloc_calls[i].load(std::memory_order_relaxed);
        out->free_calls[i]         = g_ctr.free_calls[i].load(std::memory_order_relaxed);
        out->bytes_current[i]      = g_ctr.bytes_current[i].load(std::memory_order_relaxed);
        out->bytes_total_alloc[i]  = g_ctr.bytes_total_alloc[i].load(std::memory_order_relaxed);
        out->bytes_total_freed[i]  = g_ctr.bytes_total_freed[i].load(std::memory_order_relaxed);
        out->simulated_wait_ns[i]  = g_ctr.simulated_wait_ns[i].load(std::memory_order_relaxed);
    }
}

extern "C" int ta_stats_json(char* buf, unsigned long long n) {
    ta_stats_snapshot_t s{};
    ta_get_stats(&s);

    std::ostringstream oss;
    oss << "{";
    auto arr = [&](const char* key, const unsigned long long a[3]){ 
        oss << "\"" << key << "\":[ " << a[0] << ", " << a[1] << ", " << a[2] << " ],";
    };
    arr("alloc_calls", s.alloc_calls);
    arr("free_calls", s.free_calls);
    arr("bytes_current", s.bytes_current);
    arr("bytes_total_alloc", s.bytes_total_alloc);
    arr("bytes_total_freed", s.bytes_total_freed);
    arr("simulated_wait_ns", s.simulated_wait_ns);

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

// Helpers
extern "C" void __ta_add_alloc(ta_tier_t t, unsigned long long sz, long wait_ns) {
    g_ctr.alloc_calls[(int)t]++;
    g_ctr.bytes_current[(int)t] += sz;
    g_ctr.bytes_total_alloc[(int)t] += sz;
    g_ctr.simulated_wait_ns[(int)t] += (unsigned long long) (wait_ns > 0 ? wait_ns : 0);
}

extern "C" void __ta_add_free(ta_tier_t t, unsigned long long sz) {
    g_ctr.free_calls[(int)t]++;
    g_ctr.bytes_current[(int)t] -= sz;
    g_ctr.bytes_total_freed[(int)t] += sz;
}
