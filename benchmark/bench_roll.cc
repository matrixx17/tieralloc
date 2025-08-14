#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "tieralloc.h"

static void print_json_line_with_prefix(const char* prefix) {
    int need = ta_stats_json(nullptr, 0);
    if (need <= 0) {
        std::printf("%s {}\n", prefix);
        return;
    }
    if (need < 4096) {
        char buf[4096];
        ta_stats_json(buf, sizeof(buf));
        std::printf("%s ", prefix);
        fwrite(buf, 1, (size_t)need, stdout);
        fputc('\n', stdout);
    } else {
        char* buf = (char*)malloc((size_t)need + 1);
        ta_stats_json(buf, (unsigned long long)need + 1);
        std::printf("%s ", prefix);
        fwrite(buf, 1, (size_t)need, stdout);
        fputc('\n', stdout);
        free(buf);
    }
}

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
            // Touch first byte so it's not COW-elided by OS
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

        char prefix[32];
        std::snprintf(prefix, sizeof(prefix), "step=%u", step);
        print_json_line_with_prefix(prefix);
    }

    for (void* p : window) ta_free(p);
    return 0;
}
