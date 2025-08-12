#include "tieralloc.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>

namespace {

using clock_t = std::chrono::steady_clock;

struct Bucket {
    double rate_Bps{0.0};
    double capacity_bytes{0.0};
    double tokens{0.0};
    clock_t::time_point last;
    long base_latency_ns{0};
    std::mutex mtx;
};

std::array<Bucket, 3> g_buckets;

inline long ns_from_seconds(double s) {
    return static_cast<long>(s * 1'000'000'000.0);
}

void init_bucket(ta_tier_t t, double bw_Bps, long base_lat_ns) {
    auto idx = static_cast<size_t>(t);
    auto& b = g_buckets[idx];
    b.rate_Bps = bw_Bps;
    b.capacity_bytes = std::max(1.0, bw_Bps * 0.010); // ~10ms burst
    b.tokens = b.capacity_bytes;
    b.last = clock_t::now();
    b.base_latency_ns = base_lat_ns;
}

} // namespace

extern "C" void ta_set_default_config(void) {
    init_bucket(TA_TIER_FAST,   50.0 * 1024 * 1024 * 1024,  2'000);
    init_bucket(TA_TIER_NORMAL, 20.0 * 1024 * 1024 * 1024,  8'000);
    init_bucket(TA_TIER_SLOW,    5.0 * 1024 * 1024 * 1024, 40'000);
}

extern "C" long ta_charge_bytes(ta_tier_t tier, unsigned long long bytes, ta_charge_info_t* info) {
    auto& b = g_buckets[static_cast<size_t>(tier)];
    std::scoped_lock lk(b.mtx);

    const auto now   = clock_t::now();
    const double dt  = std::chrono::duration<double>(now - b.last).count();
    b.last = now;

    b.tokens = std::min(b.capacity_bytes, b.tokens + b.rate_Bps * dt);

    long wait_ns = b.base_latency_ns;

    if (static_cast<double>(bytes) <= b.tokens) {
        b.tokens -= static_cast<double>(bytes);
    } else {
        const double deficit = static_cast<double>(bytes) - b.tokens;
        const double w_sec   = deficit / b.rate_Bps;
        wait_ns += ns_from_seconds(w_sec);
        b.tokens = 0.0;
    }

    if (info) info->simulated_wait_ns += wait_ns;
    return wait_ns;
}
