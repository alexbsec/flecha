#include <cstdio>
extern "C" int none_comparison_test();
int main() {
    const int result = none_comparison_test();
    if (result != 42) {
        std::fprintf(stderr, "none comparison test failed at check %d\n", result);
    }
    return result;
}
