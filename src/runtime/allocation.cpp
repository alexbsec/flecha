#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

#include "runtime/runtime_abi.h"

extern "C" void* flecha_runtime_v1_alloc(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;
    alignment = std::max(alignment, alignof(void*));
    size = std::max(size, size_t{1});
    const size_t maximum = std::numeric_limits<size_t>::max();
    if (alignment - 1 > maximum - sizeof(void*)) return nullptr;
    const size_t overhead = sizeof(void*) + alignment - 1;
    if (size > maximum - overhead) return nullptr;
    void* base = std::malloc(size + overhead);
    if (base == nullptr) return nullptr;
    void* data = static_cast<char*>(base) + sizeof(void*);
    size_t available = size + alignment - 1;
    if (std::align(alignment, size, data, available) == nullptr) {
        std::free(base);
        return nullptr;
    }
    static_cast<void**>(data)[-1] = base;
    std::memset(data, 0, size);
    return data;
}

extern "C" void flecha_runtime_v1_free(void* data) {
    if (data != nullptr) std::free(static_cast<void**>(data)[-1]);
}
