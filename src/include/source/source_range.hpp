#ifndef FLECHA_SOURCE_SOURCE_RANGE_HPP
#define FLECHA_SOURCE_SOURCE_RANGE_HPP

#include <cstddef>

#include "source_location.hpp"

namespace flecha::source {

// Source ranges are half-open: start is included and end is excluded.
struct SourceRange {
    SourceLocation start;
    SourceLocation end;

    [[nodiscard]] constexpr bool Empty() const {
        return start.offset == end.offset;
    }

    [[nodiscard]] constexpr std::size_t Length() const {
        return end.offset - start.offset;
    }

    auto operator<=>(const SourceRange&) const = default;
};

}  // namespace flecha::source

#endif  // FLECHA_SOURCE_SOURCE_RANGE_HPP
