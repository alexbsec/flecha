#ifndef FLECHA_RUNTIME_RUNTIME_ABI_H
#define FLECHA_RUNTIME_RUNTIME_ABI_H

#include <stddef.h>
#include <stdint.h>

#define FLECHA_RUNTIME_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

void flecha_runtime_v1_stdout_string_view(const char* data, size_t length);
void flecha_runtime_v1_stdout_i8(int8_t value);
void flecha_runtime_v1_stdout_i16(int16_t value);
void flecha_runtime_v1_stdout_i32(int32_t value);
void flecha_runtime_v1_stdout_i64(int64_t value);
void flecha_runtime_v1_stdout_isize(intptr_t value);
void flecha_runtime_v1_stdout_u8(uint8_t value);
void flecha_runtime_v1_stdout_u16(uint16_t value);
void flecha_runtime_v1_stdout_u32(uint32_t value);
void flecha_runtime_v1_stdout_u64(uint64_t value);
void flecha_runtime_v1_stdout_usize(uintptr_t value);
void flecha_runtime_v1_stdout_f32(float value);
void flecha_runtime_v1_stdout_f64(double value);
void flecha_runtime_v1_stdout_bool(uint8_t value);
void flecha_runtime_v1_stdout_char(uint8_t value);
void flecha_runtime_v1_stdout_newline(void);
uint8_t flecha_runtime_v1_string_view_at(const char* data, size_t length,
                                         uint32_t index);
void flecha_runtime_v1_string_view_check_substr(size_t length, uint32_t start,
                                                uint32_t count);

#ifdef __cplusplus
}
#endif

#endif  // FLECHA_RUNTIME_RUNTIME_ABI_H
