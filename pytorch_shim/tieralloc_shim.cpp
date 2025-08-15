// Libs for binding with Python and PyTorch
#include <torch/extension.h>
#include <ATen/ATen.h>
#include <ATen/core/Allocator.h>
#include <c10/core/Device.h>
#include <c10/core/DeviceType.h>
#include <c10/core/CPUAllocator.h>
#include <pybind11/pybind11.h>

// Good old C++ libraries
#include <atomic>
#include <mutex>
#include <optional> 
#include <string> 
#include <unordered_map>

// API
extern "C" {
  const char* ta_hello(void);
  void ta_init_from_env(void);
  void* ta_alloc(unsigned long long bytes, int hint);
  void ta_free(void* p);
}

// Map strings
enum class Hint : int { DEFAULT=0, HOT=1, WARM=2, COLD=3, PIN_FAST=4, PREFER_FAST=5 };

static std::atomic<int> g_default_hint{(int)Hint::WARM};
static std::atomic<bool> g_enabled{false};

// Context for deleter
struct TAContext {
  void* ptr{nullptr};
  size_t size{0};
  int hint{(int)Hint::WARM};
};

// Implement at::Allocator
struct TierAllocAllocator final : at::Allocator {
  at::DataPtr allocate(size_t nbytes) const override {
    if (!g_enabled.load(std::memory_order_relaxed)) {
      void* p = aligned_alloc(64, ((nbytes + 63) / 64) * 64);
      if (!p) AT_ERROR("fallback aligned_alloc failed");
      return {p, p, &TierAllocAllocator::raw_delete_fallback, at::Device(at::kCPU)};
    }

    // Choose hint
    int hint = g_default_hint.load(std::memory_order_relaxed);

    // call to allocator
    void* p = ta_alloc((unsigned long long)nbytes, hint);
    if (!p) AT_ERROR("tieralloc: allocation failed for ", nbytes, "bytes");

    // Allocate context for deleter
    auto* ctx = new TAContext{p, nbytes, hint};

    // Return a DataPtr with custom deleter
    return {p, (void*)ctx, &TierAllocAllocator::raw_delete_tieralloc, at::Device(at::kCPU)};
  }

  DeleterFnPtr raw_deleter() const override {
    return &TierAllocAllocator::raw_delete_tieralloc;
  }

  static void raw_delete_tieralloc(void* ctx_void) {
    if (!ctx_void) return;
    TAContext* ctx = reinterpret_cast<TAContext*>(ctx_void);
    if (ctx->ptr) {
      ta_free(ctx->ptr);
      ctx->ptr = nullptr;
    }
    delete ctx;
  }

  static void raw_delete_fallback(void* ptr) {
    if (!ptr) return;
    free(ptr);
  }
};

static TierAllocAllocator g_tieralloc;

// Install as PyTorch CPU allocator
static void set_as_cpu_allocator() {
  // Initialize
  ta_init_from_env();
  // Higher number "wins" if multiple
  at::SetCPUAllocator(&g_tieralloc, /*priority=*/10);
}

// pybind
namespace py = pybind11;

PYBIND11_MODULE(tieralloc_shim, m) {
  m.doc() = "tieralloc PyTorch CPU allocator shim";

  m.def("enable", []() {
    if (!g_enabled.exchange(true)) {
      set_as_cpu_allocator();
    }
  }, "Enable tieralloc as the CPU allocator for tensors");

  m.def("hello", []() {
    return std::string(ta_hello());
  });

  m.def("set_default_hint", [](const std::string& s) {
    int h = (int)Hint::WARM;
    if (s == "hot") h = (int)Hint::HOT;
    else if (s == "warm") h = (int)Hint::WARM;
    else if (s == "cold") h = (int)Hint::COLD;
    else if (s == "pin_fast") h = (int)Hint::PIN_FAST;
    else if (s == "prefer_fast") h = (int)Hint::PREFER_FAST;
    g_default_hint.store(h, std::memory_order_relaxed);
  }, "Set default hint for subsequent tensor allocations: hot | warm | cold | pin_fast | prefer_fast");

}
