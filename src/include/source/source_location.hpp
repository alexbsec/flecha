#ifndef FLECHA_SOURCE_SOURCE_LOCATION_HPP
#define FLECHA_SOURCE_SOURCE_LOCATION_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>

namespace flecha::source {

// Offsets count bytes from the beginning of the source. Lines and columns are
// one-based so they can be displayed directly in diagnostics.
struct SourceLocation {
    std::size_t offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::string file;

    std::strong_ordering operator<=>(const SourceLocation&) const = default;
};

}  // namespace flecha::source

#endif  // FLECHA_SOURCE_SOURCE_LOCATION_HPP
