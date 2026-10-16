#include <cstdio>

extern "C" int structure_zero_test();

int main() {
    const int result = structure_zero_test();
    if (result != 42) {
        std::fprintf(stderr, "structure zero test failed at check %d\n",
                     result);
    }
    return result;
}
