#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "tieralloc.h"

static void print_stats() {
    char buf[2048];
    int need = ta_stats_json(buf, sizeof(buf));
    if (need >= (int)sizeof(buf)) {
        char* big = (char*)std::malloc(need+1);
        ta_stats_json(big, need+1);
        std::puts(big);
        std::free(big);
    } else {
        std::puts(buf);
    }
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

