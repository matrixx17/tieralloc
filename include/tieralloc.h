#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// --- Tiers & Hints ---
typedef enum { TA_TIER_FAST=0, TA_TIER_NORMAL=1, TA_TIER_SLOW=2 } ta_tier_t;
typedef enum {
    TA_HINT_DEFAULT=0,
    TA_HINT_HOT,
    TA_HINT_WARM,
    TA_HINT_COLD,
    TA_HINT_PIN_FAST,
    TA_HINT_PREFER_FAST
} ta_hint_t;

// --- Config ---
typedef struct {
    double bandwidth_Bps;                 // bytes per second
    long   base_latency_ns;               // extra fixed latency
    unsigned long long capacity_bytes;    // optional soft cap (later)
} ta_tier_cfg_t;

// --- Accounting structs ---
typedef struct {
    long simulated_wait_ns;
} ta_charge_info_t;

typedef struct {
    // per-tier counters (monotonic totals)
    unsigned long long alloc_calls[3];
    unsigned long long free_calls[3];
    unsigned long long bytes_current[3];        // current residency
    unsigned long long bytes_total_alloc[3];    // sum of sizes allocated
    unsigned long long bytes_total_freed[3];
    unsigned long long simulated_wait_ns[3];    // accumulated waits
} ta_stats_snapshot_t;

// --- Public API ---
void ta_init_from_env(void);
void ta_set_default_config(void);

// Throttle charging primitive (bandwidth+latency model)
long ta_charge_bytes(ta_tier_t tier, unsigned long long bytes, ta_charge_info_t* info);

// Explicit allocation API
void* ta_alloc(unsigned long long bytes, ta_hint_t hint);
void  ta_free(void* p);

// Advisory + info
int   ta_tier_of(const void* p, ta_tier_t* out_tier);
int   ta_advise(void* p, ta_hint_t hint); // P0: no-op placeholder

// Throttled "migration" primitive (returns new ptr; old ptr invalid after)
void* ta_move(void* p, ta_tier_t dst_tier);

// Stats
void  ta_get_stats(ta_stats_snapshot_t* out);
int   ta_stats_json(char* buf, unsigned long long n);

// Utility probe
const char* ta_hello(void);

#ifdef __cplusplus
}
#endif
