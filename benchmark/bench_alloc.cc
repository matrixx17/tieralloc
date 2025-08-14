#include <cstdio>
#include <vector>
#include <cstdlib>
#include "tieralloc.h"

static void print_stats_json() {
    int need = ta_stats_json(nullptr, 0);
    if (need <= 0) { std::puts("{}"); return; }
    if (need < 4096) {
        char buf[4096];
        ta_stats_json(buf, sizeof(buf));
        fwrite(buf, 1, (size_t)need, stdout);
        fputc('\n', stdout);
    } else {
        char* buf = (char*)malloc((size_t)need + 1);
        ta_stats_json(buf, (unsigned long long)need + 1);
        fwrite(buf, 1, (size_t)need, stdout);
        fputc('\n', stdout);
        free(buf);
    }
}

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

    // Print stats 
    print_stats_json();

    // Free rest
    for (unsigned i=1;i<N;i+=2) ta_free(ptrs[i]);
    return 0;
}
