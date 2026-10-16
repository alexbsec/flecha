#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

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

TEST(RuntimeStringViewTests, AcceptsNullStorageForEmptyOutput) {
    EXPECT_NO_FATAL_FAILURE(flecha_runtime_v1_stdout_string_view(nullptr, 0));
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

TEST(RuntimeFilesystemTests, ReadsEmptyAndBinaryFiles) {
    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "flecha_runtime_read_file_test.bin";
    const std::string bytes{"A\0B\n", 4};
    {
        std::ofstream stream(path, std::ios::binary);
        ASSERT_TRUE(stream.good());
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    FlechaRuntimeString output{};
    const std::string nativePath = path.string();
    EXPECT_EQ(flecha_runtime_v1_read_file(&output, nativePath.data(),
                                          nativePath.size()),
              FLECHA_RUNTIME_READ_FILE_OK);
    ASSERT_NE(output.data, nullptr);
    EXPECT_EQ(output.length, bytes.size());
    EXPECT_EQ(std::memcmp(output.data, bytes.data(), bytes.size()), 0);
    EXPECT_EQ(output.data[output.length], '\0');
    flecha_runtime_v1_string_destroy(output.data);

    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(stream.good());
    }
    EXPECT_EQ(flecha_runtime_v1_read_file(&output, nativePath.data(),
                                          nativePath.size()),
              FLECHA_RUNTIME_READ_FILE_OK);
    ASSERT_NE(output.data, nullptr);
    EXPECT_EQ(output.length, 0);
    EXPECT_EQ(output.data[0], '\0');
    flecha_runtime_v1_string_destroy(output.data);
    EXPECT_TRUE(std::filesystem::remove(path));
}

TEST(RuntimeFilesystemTests, RejectsInvalidAndMissingPaths) {
    FlechaRuntimeString output{reinterpret_cast<char*>(1), 1, 1};
    const char invalid[] = {'a', '\0', 'b'};
    EXPECT_EQ(flecha_runtime_v1_read_file(&output, invalid, sizeof(invalid)),
              FLECHA_RUNTIME_READ_FILE_INVALID_PATH);
    EXPECT_EQ(output.data, nullptr);
    EXPECT_EQ(output.length, 0);

    const std::string missing = (std::filesystem::temp_directory_path() /
                                 "flecha_file_that_does_not_exist_7d12")
                                    .string();
    EXPECT_EQ(
        flecha_runtime_v1_read_file(&output, missing.data(), missing.size()),
        FLECHA_RUNTIME_READ_FILE_NOT_FOUND);
    EXPECT_EQ(output.data, nullptr);
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

TEST(RuntimeAllocationTests, AllocatesZeroedRuntimeElementCounts) {
    auto* elements = static_cast<int32_t*>(
        flecha_runtime_v1_alloc_array(37, sizeof(int32_t), alignof(int32_t)));
    ASSERT_NE(elements, nullptr);
    for (size_t i = 0; i < 37; ++i) {
        EXPECT_EQ(elements[i], 0);
        elements[i] = static_cast<int32_t>(i);
    }
    EXPECT_EQ(elements[36], 36);
    EXPECT_EQ(flecha_runtime_v1_allocation_count(elements), 37u);
    flecha_runtime_v1_free(elements);
    void* empty = flecha_runtime_v1_alloc_array(0, sizeof(int32_t), 64);
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(empty) % 64, 0u);
    EXPECT_EQ(flecha_runtime_v1_allocation_count(empty), 0u);
    EXPECT_EQ(flecha_runtime_v1_allocation_count(nullptr), 0u);
    flecha_runtime_v1_free(empty);
}

TEST(RuntimeAllocationTests, RejectsArrayOverflowBeforeAllocating) {
    EXPECT_EQ(flecha_runtime_v1_alloc_array(SIZE_MAX / 4 + 1, 4, 4), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc_array(SIZE_MAX, 2, 8), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc_array(1, 0, 8), nullptr);
    EXPECT_EQ(flecha_runtime_v1_alloc_array(0, 4, 3), nullptr);
}
