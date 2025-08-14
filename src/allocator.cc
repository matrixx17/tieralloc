#include "tieralloc.h"

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <unordered_map>
#include <mutex>
#include <new>

extern "C" void ta_set_default_config(void); 
extern "C" ta_tier_t __ta_pick_tier_from_hint(ta_hint_t hint);
extern "C" void __ta_add_alloc(ta_tier_t, unsigned long long, long);
extern "C" void __ta_add_free(ta_tier_t, unsigned long long);

namespace {

struct Rec {
    unsigned long long size;
    ta_tier_t tier;
};

std::unordered_map<void*, Rec> g_map;
std::mutex g_map_mtx;

inline unsigned long long round_up_pages(unsigned long long n) {
    unsigned long long page = (unsigned long long) sysconf(_SC_PAGESIZE);
    return (n + page - 1) / page * page;
}

} 

extern "C" void ta_init_from_env(void) {
    
    ta_set_default_config();
}

extern "C" const char* ta_hello(void) {
    return "tieralloc-ok";
}

extern "C" void* ta_alloc(unsigned long long bytes, ta_hint_t hint) {
    ta_tier_t tier = __ta_pick_tier_from_hint(hint);

    // Simulate cost before allocation
    ta_charge_info_t info{0};
    long wait_ns = ta_charge_bytes(tier, bytes, &info);
    (void)wait_ns; 

    unsigned long long sz = round_up_pages(bytes);
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* p = mmap(nullptr, sz, prot, flags, -1, 0);
    if (p == MAP_FAILED) return nullptr;

    {
        std::scoped_lock lk(g_map_mtx);
        g_map[p] = Rec{sz, tier};
    }

    __ta_add_alloc(tier, sz, info.simulated_wait_ns);
    return p;
}

extern "C" void ta_free(void* p) {
    if (!p) return;
    Rec rec{};
    {
        std::scoped_lock lk(g_map_mtx);
        auto it = g_map.find(p);
        if (it == g_map.end()) { 
            return;
        }
        rec = it->second;
        g_map.erase(it);
    }
    munmap(p, rec.size);
    __ta_add_free(rec.tier, rec.size);
}

extern "C" int ta_tier_of(const void* p, ta_tier_t* out_tier) {
    if (!p || !out_tier) return -1;
    std::scoped_lock lk(g_map_mtx);
    auto it = g_map.find(const_cast<void*>(p));
    if (it == g_map.end()) return -2;
    *out_tier = it->second.tier;
    return 0;
}

extern "C" int ta_advise(void* /*p*/, ta_hint_t /*hint*/) { 
    return 0;
}

extern "C" void* ta_move(void* p, ta_tier_t dst_tier) {
    if (!p) return nullptr;
    Rec rec{};
    {
        std::scoped_lock lk(g_map_mtx);
        auto it = g_map.find(p);
        if (it == g_map.end()) return nullptr;
        rec = it->second;
    }

    // Charge read from src, write to dst
    ta_charge_info_t info{0};
    (void) ta_charge_bytes(rec.tier, rec.size, &info);
    (void) ta_charge_bytes(dst_tier, rec.size, &info);

    // Allocate new region in dst tier
    void* q = ta_alloc(rec.size, dst_tier == TA_TIER_FAST ? TA_HINT_PIN_FAST :
                                  dst_tier == TA_TIER_NORMAL ? TA_HINT_WARM : TA_HINT_COLD);
    if (!q) return nullptr;

    // Copy + free old
    memcpy(q, p, (size_t)rec.size);
    ta_free(p);
    return q;
}
