#include <cstdint>
#include <limits>

extern "C" {
void increment_ref(std::uint8_t* value);
std::uint8_t read_ref(const std::uint8_t* value);
std::uint8_t exercise_references();
std::uint8_t exercise_reference_flow(bool choice);
std::uint8_t exercise_pointer_ref(std::uint8_t* first, std::uint8_t* second);
std::int8_t signed_byte_minimum();
std::int64_t signed_minimum();
std::uint64_t unsigned_maximum();
}

int main() {
    std::uint8_t value = 41;
    increment_ref(&value);
    if (value != 42 || read_ref(&value) != 42) return 1;
    if (exercise_references() != 42) return 2;
    if (exercise_reference_flow(false) != 42) return 3;
    if (exercise_reference_flow(true) != 42) return 4;
    std::uint8_t other = 0;
    if (exercise_pointer_ref(&value, &other) != 42 || other != 42) return 5;
    if (signed_byte_minimum() != std::numeric_limits<std::int8_t>::min())
        return 6;
    if (signed_minimum() != std::numeric_limits<std::int64_t>::min()) return 7;
    if (unsigned_maximum() != std::numeric_limits<std::uint64_t>::max())
        return 8;
    return 42;
}
