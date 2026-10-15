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

TEST(RuntimeStringTests, OwnsClonesAndDestroysIndependentStorage) {
    FlechaRuntimeString first{};
    flecha_runtime_v1_string_from_view(&first, "hi mom", 6);
    ASSERT_NE(first.data, nullptr);
    EXPECT_EQ(first.length, 6);
    EXPECT_EQ(first.capacity, 6);
    EXPECT_STREQ(first.data, "hi mom");

    FlechaRuntimeString copy{};
    flecha_runtime_v1_string_clone(&copy, first.data, first.length);
    ASSERT_NE(copy.data, nullptr);
    EXPECT_NE(copy.data, first.data);
    EXPECT_EQ(copy.length, first.length);
    EXPECT_STREQ(copy.data, first.data);

    flecha_runtime_v1_string_destroy(copy.data);
    flecha_runtime_v1_string_destroy(first.data);
}

TEST(RuntimeStringTests, EmptyStringNeedsNoAllocation) {
    FlechaRuntimeString empty{};
    flecha_runtime_v1_string_from_view(&empty, "", 0);
    EXPECT_EQ(empty.data, nullptr);
    EXPECT_EQ(empty.length, 0);
    EXPECT_EQ(empty.capacity, 0);
    flecha_runtime_v1_string_destroy(empty.data);
}

TEST(RuntimeStringTests, ConcatenatesIndependentOwnedStorage) {
    FlechaRuntimeString joined{};
    flecha_runtime_v1_string_concat(&joined, "fle", 3, "cha", 3);
    ASSERT_NE(joined.data, nullptr);
    EXPECT_EQ(joined.length, 6);
    EXPECT_EQ(joined.capacity, 6);
    EXPECT_STREQ(joined.data, "flecha");
    flecha_runtime_v1_string_destroy(joined.data);
}

TEST(RuntimeAllocationTests, ZeroesStorageAndHonorsAlignment) {
    for (const size_t alignment :
         {size_t{1}, size_t{8}, size_t{64}, size_t{4096}}) {
        void* data = flecha_runtime_v1_alloc(137, alignment);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(data) % alignment, 0);
        for (size_t i = 0; i < 137; ++i)
            EXPECT_EQ(static_cast<unsigned char*>(data)[i], 0);
        static_cast<unsigned char*>(data)[136] = 42;
        flecha_runtime_v1_free(data);
    }
    void* empty = flecha_runtime_v1_alloc(0, 1);
    ASSERT_NE(empty, nullptr);
    flecha_runtime_v1_free(empty);
    flecha_runtime_v1_free(nullptr);
}

TEST(RuntimeAllocationTests, RejectsInvalidAlignmentAndSizeOverflow) {
    EXPECT_EQ(flecha_runtime_v1_alloc(1, 0), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc(1, 3), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc(SIZE_MAX, 8), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc(SIZE_MAX - sizeof(void*), 64), nullptr);
}
