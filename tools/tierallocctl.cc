#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "tieralloc.h"

static void print_stats() { 
    int need = ta_stats_json(nullptr, 0);
    if (need <= 0) { std::puts("{}"); return; }

    // Allocate exact buffer +1 for safety
    char* buf = (char*)std::malloc((size_t)need + 1);
    if (!buf) { std::puts("{}"); return; }
    ta_stats_json(buf, (unsigned long long)need + 1);

    fwrite(buf, 1, (size_t)need, stdout);
    fputc('\n', stdout);
    std::free(buf);
}

int main(int argc, char** argv) {
    ta_init_from_env();
    if (argc > 1 && std::strcmp(argv[1], "stats")==0) {
        print_stats();
        return 0;
    }
    std::printf("%s\n", ta_hello());
    print_stats();
    return 0;
}
