#include "runtime/runtime_abi.h"

#include <cinttypes>
#include <cstdlib>
#include <cstdio>

extern "C" {

void flecha_runtime_v1_stdout_string_view(const char* data, size_t length) {
    (void)std::fwrite(data, 1, length, stdout);
}

void flecha_runtime_v1_stdout_i8(int8_t value) {
    (void)std::fprintf(stdout, "%" PRId8, value);
}

void flecha_runtime_v1_stdout_i16(int16_t value) {
    (void)std::fprintf(stdout, "%" PRId16, value);
}

void flecha_runtime_v1_stdout_i32(int32_t value) {
    (void)std::fprintf(stdout, "%" PRId32, value);
}

void flecha_runtime_v1_stdout_i64(int64_t value) {
    (void)std::fprintf(stdout, "%" PRId64, value);
}

void flecha_runtime_v1_stdout_isize(intptr_t value) {
    (void)std::fprintf(stdout, "%" PRIdPTR, value);
}

void flecha_runtime_v1_stdout_u8(uint8_t value) {
    (void)std::fprintf(stdout, "%" PRIu8, value);
}

void flecha_runtime_v1_stdout_u16(uint16_t value) {
    (void)std::fprintf(stdout, "%" PRIu16, value);
}

void flecha_runtime_v1_stdout_u32(uint32_t value) {
    (void)std::fprintf(stdout, "%" PRIu32, value);
}

void flecha_runtime_v1_stdout_u64(uint64_t value) {
    (void)std::fprintf(stdout, "%" PRIu64, value);
}

void flecha_runtime_v1_stdout_usize(uintptr_t value) {
    (void)std::fprintf(stdout, "%" PRIuPTR, value);
}

void flecha_runtime_v1_stdout_f32(float value) {
    (void)std::fprintf(stdout, "%.9g", static_cast<double>(value));
}

void flecha_runtime_v1_stdout_f64(double value) {
    (void)std::fprintf(stdout, "%.17g", value);
}

void flecha_runtime_v1_stdout_bool(uint8_t value) {
    const char* text = value == 0 ? "false" : "true";
    const size_t length = value == 0 ? 5 : 4;
    (void)std::fwrite(text, 1, length, stdout);
}

void flecha_runtime_v1_stdout_char(uint8_t value) {
    (void)std::fputc(value, stdout);
}

void flecha_runtime_v1_stdout_newline(void) {
    (void)std::fputc('\n', stdout);
}

uint8_t flecha_runtime_v1_string_view_at(const char* data, size_t length,
                                         uint32_t index) {
    if (index >= length) {
        (void)std::fprintf(
            stderr,
            "Flecha runtime error: stringv index %" PRIu32
            " is out of bounds for length %zu\n",
            index, length);
        std::abort();
    }
    return static_cast<uint8_t>(data[index]);
}

void flecha_runtime_v1_string_view_check_substr(size_t length, uint32_t start,
                                                uint32_t count) {
    if (start > length || count > length - start) {
        (void)std::fprintf(
            stderr,
            "Flecha runtime error: stringv substring (%" PRIu32 ", %" PRIu32
            ") is out of bounds for length %zu\n",
            start, count, length);
        std::abort();
    }
}

}  // extern "C"
