#include <cstddef>
#include <limits>

extern "C" int slice_test();
extern "C" int slice_range_test();
extern "C" int slice_read(std::size_t);
extern "C" void slice_write(std::size_t);
extern "C" void slice_borrow(std::size_t);
extern "C" std::size_t slice_bounds(std::size_t, std::size_t);
extern "C" std::size_t slice_nested(std::size_t);
extern "C" int slice_front(std::size_t);
extern "C" void slice_back(std::size_t);
extern "C" void slice_increment(std::size_t);

int main(int argc, char** argv) {
    if (argc > 1) {
        switch (argv[1][0]) {
            case 'r':
                return slice_read(2);
            case 'w':
                slice_write(2);
                break;
            case 'b':
                slice_borrow(2);
                break;
            case 'o':
                return static_cast<int>(slice_bounds(2, 1));
            case 'e':
                return static_cast<int>(slice_bounds(0, 4));
            case 'n':
                return static_cast<int>(slice_nested(3));
            case 'f':
                return slice_front(0);
            case 'k':
                slice_back(0);
                break;
            case 'u':
                slice_increment(3);
                break;
            case 'm':
                return slice_read(std::numeric_limits<std::size_t>::max());
            default:
                return 99;
        }
        return 0;
    }
    if (slice_test() != 42) return 1;
    if (slice_range_test() != 42) return 2;
    if (slice_bounds(3, 3) != 0) return 3;
    if (slice_read(1) != 3) return 4;
    if (slice_front(1) != 1) return 5;
    return 42;
}
