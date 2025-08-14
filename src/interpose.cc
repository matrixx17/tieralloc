#define _GNU_SOURCE
#include "tieralloc.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Enable with: TA_INTERPOSE=1 LD_PRELOAD=./libtieralloc.so <prog>
// Escape hatch at runtime with TA_DISABLE=1.

static bool g_interpose = false;
static bool g_disabled = false;
static thread_local int g_in_hook = 0;

static void init_flags(void) __attribute__((constructor));
static void init_flags(void) {
    const char* x = getenv("TA_INTERPOSE");
    g_interpose = (x && *x == '1');
    const char* y = getenv("TA_DISABLE");
    g_disabled = (y && *y == '1');
    ta_init_from_env();
}

static void* (*real_malloc)(size_t) = NULL;
static void  (*real_free)(void*) = NULL;
static void* (*real_calloc)(size_t,size_t) = NULL;
static void* (*real_realloc)(void*,size_t) = NULL;

extern "C" int __ta_internal_get_size(const void* p, unsigned long long* out_size);

static void resolve_libc(void) {
    if (!real_malloc) real_malloc = (void*(*)(size_t)) dlsym(RTLD_NEXT, "malloc");
    if (!real_free)   real_free   = (void (*)(void*)) dlsym(RTLD_NEXT, "free");
    if (!real_calloc) real_calloc = (void*(*)(size_t,size_t)) dlsym(RTLD_NEXT, "calloc");
    if (!real_realloc) real_realloc = (void*(*)(void*,size_t)) dlsym(RTLD_NEXT, "realloc");
}

static const size_t TA_MIN_ROUTE = 64 * 1024; // 64KB

extern "C" void* malloc(size_t n) {
    resolve_libc();
    if (!g_interpose || g_disabled || g_in_hook) return real_malloc(n);
    g_in_hook++;
    void* p = (n >= TA_MIN_ROUTE) ? ta_alloc(n, TA_HINT_DEFAULT) : real_malloc(n);
    g_in_hook--;
    return p;
}

extern "C" void free(void* p) {
    resolve_libc();
    if (!g_interpose || g_disabled || g_in_hook) { real_free(p); return; }
    g_in_hook++;
    ta_free(p); // ta_free will ignore pointers it doesn't own
    g_in_hook--;
}

extern "C" void* calloc(size_t a, size_t b) {
    resolve_libc();
    if (!g_interpose || g_disabled || g_in_hook) return real_calloc(a,b);
    size_t n = a * b;
    void* p = (n >= TA_MIN_ROUTE) ? ta_alloc(n, TA_HINT_DEFAULT) : real_calloc(a,b);
    if (p && n >= TA_MIN_ROUTE) memset(p, 0, n);
    return p;
}
 
extern "C" void* realloc(void* p, size_t n) {
  resolve_libc();
  if (!g_interpose || g_disabled || g_in_hook) return real_realloc(p, n);
  if (!p) return malloc(n);
  if (n == 0) {
    free(p);
    return NULL;
  }

  ta_tier_t t;
  if (ta_tier_of(p, &t) == 0) {
    void* q = ta_alloc(n, TA_HINT_DEFAULT);
    if (!p) return NULL;

    unsigned long long old_sz = 0;
    size_t copy_n = 0;
    if (__ta_internal_get_size(p, &old_sz) == 0) {
      copy_n = (size_t)((old_sz < (unsigned long long)n) ? old_sz : (unsigned long long)n);
    }
    if (copy_n > 0) memcpy(q, p, copy_n);

    ta_free(p);
    return q;
  } else {
    return real_realloc(p, n);
  }
}

