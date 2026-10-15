#include <cstddef>
#include <cstdio>

extern "C" int defer_order_test();
extern "C" int defer_loop_test();
extern "C" int defer_jump_test();
extern "C" int defer_capture_test();
extern "C" int defer_stack_test();
extern "C" void defer_trap_test(std::size_t);
extern "C" void defer_argument_trap_test(std::size_t);
extern "C" void defer_cleanup_trap_test(std::size_t);

int main(int argc, char** argv) {
    // Make any incorrectly executed deferred output observable before a trap.
    std::setbuf(stdout, nullptr);
    if (argc > 1) {
        switch (argv[1][0]) {
            case 't':
                defer_trap_test(1);
                break;
            case 'a':
                defer_argument_trap_test(1);
                break;
            case 'c':
                defer_cleanup_trap_test(1);
                break;
            default:
                return 99;
        }
        return 0;
    }
    if (defer_order_test() != 42) return 1;
    if (defer_loop_test() != 42) return 2;
    if (defer_jump_test() != 42) return 3;
    if (defer_capture_test() != 42) return 4;
    if (defer_stack_test() != 42) return 5;
    return 42;
}
