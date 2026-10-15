extern "C" int tuple_values_test();
extern "C" int tuple_aggregate_test();
extern "C" int tuple_strings_test();
extern "C" int tuple_cleanup_test();
extern "C" int tuple_bare_bindings_test();

int main() {
    if (tuple_values_test() != 42) return 1;
    if (tuple_aggregate_test() != 42) return 2;
    if (tuple_strings_test() != 42) return 3;
    if (tuple_cleanup_test() != 42) return 4;
    if (tuple_bare_bindings_test() != 42) return 5;
    return 42;
}
