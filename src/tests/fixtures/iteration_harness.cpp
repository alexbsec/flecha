extern "C" int range_test();
extern "C" int counted_test();
extern "C" int increment_test();
extern "C" int early_return_test();
extern "C" int jump_test();

int main() {
    if (range_test() != 42) return 1;
    if (counted_test() != 42) return 2;
    if (increment_test() != 42) return 3;
    if (early_return_test() != 42) return 4;
    if (jump_test() != 42) return 5;
    return 42;
}
