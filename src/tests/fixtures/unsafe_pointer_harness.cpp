#include <cstdint>

extern "C" {
std::int32_t increment(std::int32_t* pointer);
std::int32_t read_only(const std::int32_t* pointer);
std::int32_t* replace(std::int32_t** slot, std::int32_t* next);
bool is_null(std::int32_t* pointer);
std::int32_t exercise_locals();
std::int32_t exercise_scopes();
}

int main() {
    std::int32_t value = 40;
    if (increment(&value) != 42 || value != 42) return 1;
    const std::int32_t fixed = 42;
    if (read_only(&fixed) != 42) return 2;
    std::int32_t* slot = nullptr;
    if (!is_null(slot)) return 3;
    if (replace(&slot, &value) != &value || slot != &value) return 4;
    if (is_null(slot)) return 5;
    if (exercise_locals() != 42) return 6;
    if (exercise_scopes() != 42) return 7;
    return 42;
}
