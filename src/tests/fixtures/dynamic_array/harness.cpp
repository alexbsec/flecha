#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <set>
extern "C" void* __real_flecha_runtime_v1_alloc_array(std::size_t, std::size_t,
                                                      std::size_t);
extern "C" void __real_flecha_runtime_v1_free(void*);
namespace {
struct Audit {
    std::set<void*> live;
    unsigned calls = 0;
    unsigned failures = 0;
    ~Audit() {
        if (!live.empty() || failures != 1) {
            std::fprintf(
                stderr,
                "array allocations leaked or failure injection was skipped\n");
            std::_Exit(90);
        }
    }
} audit;
}  // namespace
extern "C" void* __wrap_flecha_runtime_v1_alloc_array(std::size_t count,
                                                      std::size_t size,
                                                      std::size_t alignment) {
    if (++audit.calls == 2) {
        ++audit.failures;
        return nullptr;
    }
    void* data = __real_flecha_runtime_v1_alloc_array(count, size, alignment);
    if (data) audit.live.insert(data);
    return data;
}
extern "C" void __wrap_flecha_runtime_v1_free(void* data) {
    if (data && audit.live.erase(data) != 1) {
        std::fprintf(stderr,
                     "array freed an invalid or already freed allocation\n");
        std::_Exit(91);
    }
    __real_flecha_runtime_v1_free(data);
}
