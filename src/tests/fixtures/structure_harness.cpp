#include <cstdio>
#include <initializer_list>
extern "C" int structure_methods_test();
extern "C" int structure_lifecycle_test();
extern "C" int structure_cleanup_test();
extern "C" int structure_owned_fields_test();
extern "C" int structure_defaults_test();
extern "C" int structure_defer_test();
extern "C" int structure_aggregate_lifetime_test();
extern "C" int structure_constructor_arguments_test();
extern "C" int structure_disjoint_borrows_test();
int main() {
    int index = 0;
    for (const auto test : {structure_methods_test, structure_lifecycle_test,
                            structure_cleanup_test, structure_owned_fields_test,
                            structure_defaults_test, structure_defer_test,
                            structure_aggregate_lifetime_test,
                            structure_constructor_arguments_test,
                            structure_disjoint_borrows_test}) {
        ++index;
        int result = test();
        if (result != 42) {
            std::fprintf(stderr, "structure test %d failed at check %d\n",
                         index, result);
            return index;
        }
    }
    return 42;
}
