#include <cstddef>
#include <cstdio>
#include <initializer_list>
extern "C" int allocation_values_test();
extern "C" int allocation_lifecycle_test();
extern "C" int allocation_defer_test();
extern "C" int allocation_failure_test();
extern "C" int allocation_success_test();
extern "C" void* __real_flecha_runtime_v1_alloc(std::size_t, std::size_t);
namespace {
bool failNext = false;
}
extern "C" void* __wrap_flecha_runtime_v1_alloc(std::size_t size,
                                                std::size_t alignment) {
    if (failNext) {
        failNext = false;
        return nullptr;
    }
    return __real_flecha_runtime_v1_alloc(size, alignment);
}
int main() {
    int index = 0;
    for (const auto test : {allocation_values_test, allocation_lifecycle_test,
                            allocation_defer_test, allocation_success_test}) {
        ++index;
        const int result = test();
        if (result != 42) {
            std::fprintf(stderr, "allocation test %d failed at check %d\n",
                         index, result);
            return index;
        }
    }
    failNext = true;
    if (allocation_failure_test() != 42 || failNext) return 5;
    return 42;
}
