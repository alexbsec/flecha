#ifndef FLECHA_RUNTIME_RUNTIME_ABI_H
#define FLECHA_RUNTIME_RUNTIME_ABI_H

#include <stddef.h>
#include <stdint.h>

#define FLECHA_RUNTIME_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlechaRuntimeString {
    char* data;
    size_t length;
    size_t capacity;
} FlechaRuntimeString;

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
void flecha_runtime_v1_string_from_view(FlechaRuntimeString* output,
                                        const char* data, size_t length);
void flecha_runtime_v1_string_clone(FlechaRuntimeString* output,
                                    const char* data, size_t length);
void flecha_runtime_v1_string_destroy(char* data);
void flecha_runtime_v1_string_from_i8(FlechaRuntimeString* output,
                                      int8_t value);
void flecha_runtime_v1_string_from_i16(FlechaRuntimeString* output,
                                       int16_t value);
void flecha_runtime_v1_string_from_i32(FlechaRuntimeString* output,
                                       int32_t value);
void flecha_runtime_v1_string_from_i64(FlechaRuntimeString* output,
                                       int64_t value);
void flecha_runtime_v1_string_from_isize(FlechaRuntimeString* output,
                                         intptr_t value);
void flecha_runtime_v1_string_from_u8(FlechaRuntimeString* output,
                                      uint8_t value);
void flecha_runtime_v1_string_from_u16(FlechaRuntimeString* output,
                                       uint16_t value);
void flecha_runtime_v1_string_from_u32(FlechaRuntimeString* output,
                                       uint32_t value);
void flecha_runtime_v1_string_from_u64(FlechaRuntimeString* output,
                                       uint64_t value);
void flecha_runtime_v1_string_from_usize(FlechaRuntimeString* output,
                                         uintptr_t value);
void flecha_runtime_v1_string_from_f32(FlechaRuntimeString* output,
                                       float value);
void flecha_runtime_v1_string_from_f64(FlechaRuntimeString* output,
                                       double value);
void flecha_runtime_v1_string_from_bool(FlechaRuntimeString* output,
                                        uint8_t value);
void flecha_runtime_v1_string_from_char(FlechaRuntimeString* output,
                                        uint8_t value);

#ifdef __cplusplus
}
#endif

#endif  // FLECHA_RUNTIME_RUNTIME_ABI_H
