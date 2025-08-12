#include <cstdio>
#include <vector>
#include <cstring>
#include "tieralloc.h"

int main() {
    ta_init_from_env();
    const unsigned STEPS = 10;
    const unsigned BATCH = 8;
    const unsigned long long SZ = 8ull<<20; // 8MB blocks

    std::vector<void*> window;

    for (unsigned step=0; step<STEPS; ++step) {
        // Push new hot blocks into FAST
        for (unsigned i=0;i<BATCH;i++) {
            void* p = ta_alloc(SZ, TA_HINT_HOT);
            if (!p) return 1;
            // touch first byte so it's not COW-elided by OS 
            std::memset(p, 0, 1);
            window.push_back(p);
        }

        // If window grows, demote oldest to SLOW
        if (window.size() > BATCH*2) {
            void* old = window.front(); window.erase(window.begin());
            void* q = ta_move(old, TA_TIER_SLOW);
            if (!q) return 2;
            window.push_back(q);
        }

        char buf[512];
        ta_stats_json(buf, sizeof(buf));
        std::printf("step=%u %s\n", step, buf);
    }

    for (void* p : window) ta_free(p);
    return 0;
}

