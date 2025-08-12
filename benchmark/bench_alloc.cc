#include <cstdio>
#include <vector>
#include "tieralloc.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    ta_init_from_env();

    const unsigned long long N = 64;            // blocks
    const unsigned long long SZ = 16ull<<20;    // 16MB each
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    // Allocate on different tiers
    for (unsigned i=0;i<N;i++) {
        ta_hint_t h = (i%3==0) ? TA_HINT_HOT : (i%3==1) ? TA_HINT_WARM : TA_HINT_COLD;
        void* p = ta_alloc(SZ, h);
        if (!p) { std::fprintf(stderr, "alloc failed\n"); return 1; }
        ptrs.push_back(p);
    }

    // Free half
    for (unsigned i=0;i<N;i+=2) ta_free(ptrs[i]);

    char buf[1024];
    ta_stats_json(buf, sizeof(buf));
    std::puts(buf);

    // Free rest
    for (unsigned i=1;i<N;i+=2) ta_free(ptrs[i]);
    return 0;
}

