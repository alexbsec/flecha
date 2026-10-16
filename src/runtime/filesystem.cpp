#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "runtime/runtime_abi.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int32_t MapOpenError(int error) {
    switch (error) {
        case ENOENT:
#if defined(ENOTDIR) && ENOTDIR != ENOENT
        case ENOTDIR:
#endif
            return FLECHA_RUNTIME_READ_FILE_NOT_FOUND;
        case EACCES:
#if defined(EPERM) && EPERM != EACCES
        case EPERM:
#endif
            return FLECHA_RUNTIME_READ_FILE_PERMISSION_DENIED;
#if defined(ENAMETOOLONG) && ENAMETOOLONG != EINVAL
        case ENAMETOOLONG:
#endif
        case EINVAL:
            return FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
        default:
            return FLECHA_RUNTIME_READ_FILE_FAILED;
    }
}

FILE* OpenBinary(const char* path, size_t path_length, int32_t* status) {
#ifdef _WIN32
    if (path_length > static_cast<size_t>(INT_MAX)) {
        *status = FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
        return nullptr;
    }
    const int wideLength =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path,
                            static_cast<int>(path_length), nullptr, 0);
    if (wideLength <= 0) {
        *status = FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
        return nullptr;
    }
    auto* widePath = static_cast<wchar_t*>(
        std::malloc((static_cast<size_t>(wideLength) + 1) * sizeof(wchar_t)));
    if (widePath == nullptr) {
        *status = FLECHA_RUNTIME_READ_FILE_OUT_OF_MEMORY;
        return nullptr;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path,
                            static_cast<int>(path_length), widePath,
                            wideLength) != wideLength) {
        std::free(widePath);
        *status = FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
        return nullptr;
    }
    widePath[wideLength] = L'\0';
    errno = 0;
    FILE* file = _wfopen(widePath, L"rb");
    const int openError = errno;
    std::free(widePath);
    if (file == nullptr) *status = MapOpenError(openError);
    return file;
#else
    if (path_length == std::numeric_limits<size_t>::max()) {
        *status = FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
        return nullptr;
    }
    auto* terminated = static_cast<char*>(std::malloc(path_length + 1));
    if (terminated == nullptr) {
        *status = FLECHA_RUNTIME_READ_FILE_OUT_OF_MEMORY;
        return nullptr;
    }
    std::memcpy(terminated, path, path_length);
    terminated[path_length] = '\0';
    errno = 0;
    FILE* file = std::fopen(terminated, "rb");
    const int openError = errno;
    std::free(terminated);
    if (file == nullptr) *status = MapOpenError(openError);
    return file;
#endif
}

}  // namespace

extern "C" int32_t flecha_runtime_v1_read_file(FlechaRuntimeString* output,
                                               const char* path,
                                               size_t path_length) {
    if (output == nullptr) return FLECHA_RUNTIME_READ_FILE_INVALID_PATH;
    *output = {nullptr, 0, 0};
    if (path == nullptr || path_length == 0 ||
        std::memchr(path, '\0', path_length) != nullptr)
        return FLECHA_RUNTIME_READ_FILE_INVALID_PATH;

    int32_t status = FLECHA_RUNTIME_READ_FILE_OK;
    FILE* file = OpenBinary(path, path_length, &status);
    if (file == nullptr) return status;

    constexpr size_t CHUNK_SIZE = 64 * 1024;
    constexpr size_t MAX_LENGTH =
        std::numeric_limits<size_t>::max() - 1 < UINT32_MAX
            ? std::numeric_limits<size_t>::max() - 1
            : UINT32_MAX;
    unsigned char chunk[CHUNK_SIZE];
    char* data = nullptr;
    size_t length = 0;
    size_t capacity = 0;
    while (true) {
        errno = 0;
        const size_t count = std::fread(chunk, 1, CHUNK_SIZE, file);
        if (count != 0) {
            if (count > MAX_LENGTH - length) {
                status = FLECHA_RUNTIME_READ_FILE_TOO_LARGE;
                break;
            }
            const size_t required = length + count;
            if (required > capacity) {
                size_t grown = capacity == 0 ? CHUNK_SIZE : capacity;
                while (grown < required) {
                    if (grown > MAX_LENGTH / 2) {
                        grown = MAX_LENGTH;
                        break;
                    }
                    grown *= 2;
                }
                char* replacement =
                    static_cast<char*>(std::realloc(data, grown + 1));
                if (replacement == nullptr) {
                    status = FLECHA_RUNTIME_READ_FILE_OUT_OF_MEMORY;
                    break;
                }
                data = replacement;
                capacity = grown;
            }
            std::memcpy(data + length, chunk, count);
            length = required;
        }
        if (count != CHUNK_SIZE) {
            if (std::ferror(file) != 0 && errno == EINTR) {
                std::clearerr(file);
                continue;
            }
            if (std::ferror(file) != 0)
                status = FLECHA_RUNTIME_READ_FILE_FAILED;
            break;
        }
    }

    if (std::fclose(file) != 0 && status == FLECHA_RUNTIME_READ_FILE_OK)
        status = FLECHA_RUNTIME_READ_FILE_FAILED;
    if (status != FLECHA_RUNTIME_READ_FILE_OK) {
        std::free(data);
        return status;
    }
    if (data == nullptr) {
        data = static_cast<char*>(std::malloc(1));
        if (data == nullptr) return FLECHA_RUNTIME_READ_FILE_OUT_OF_MEMORY;
    }
    data[length] = '\0';
    *output = {data, length, capacity};
    return FLECHA_RUNTIME_READ_FILE_OK;
}
