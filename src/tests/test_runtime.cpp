#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "runtime/runtime_abi.h"

TEST(RuntimeStringViewTests, ReturnsCharactersWithinBounds) {
    const char text[] = "flecha";
    EXPECT_EQ(flecha_runtime_v1_string_view_at(text, 6, 0),
              static_cast<std::uint8_t>('f'));
    EXPECT_EQ(flecha_runtime_v1_string_view_at(text, 6, 5),
              static_cast<std::uint8_t>('a'));
}

TEST(RuntimeStringViewTests, AcceptsValidSubstringBounds) {
    EXPECT_NO_FATAL_FAILURE(
        flecha_runtime_v1_string_view_check_substr(6, 1, 3));
    EXPECT_NO_FATAL_FAILURE(
        flecha_runtime_v1_string_view_check_substr(6, 6, 0));
}

TEST(RuntimeStringViewTests, RejectsInvalidBounds) {
    const char text[] = "flecha";
    EXPECT_DEATH((void)flecha_runtime_v1_string_view_at(text, 6, 6),
                 "out of bounds");
    EXPECT_DEATH(flecha_runtime_v1_string_view_check_substr(6, 5, 2),
                 "out of bounds");
}
