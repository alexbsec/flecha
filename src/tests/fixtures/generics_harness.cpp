#include <cstdio>
#include <initializer_list>
extern "C" int generic_functions_test();
extern "C" int generic_structures_test();
extern "C" int generic_ownership_test();
extern "C" int generic_runtime_array_test();
int main() {
    for (auto test : {generic_functions_test, generic_structures_test,
                      generic_ownership_test, generic_runtime_array_test}) {
        const int result = test();
        if (result != 42) {
            std::fprintf(stderr, "generic test returned %d\n", result);
            return 1;
        }
    }
    return 42;
}
