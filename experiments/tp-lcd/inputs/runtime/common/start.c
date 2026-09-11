#include <stddef.h>

extern int builder_main(int argc, char **argv);
extern char __bss_start[], __bss_end[], __tbss_start[], __tbss_end[];
extern void builder_platform_init(void);
extern void builder_platform_finish(int code) __attribute__((noreturn));

typedef void (*init_function)(void);
extern init_function __preinit_array_start[], __preinit_array_end[];
extern init_function __init_array_start[], __init_array_end[];

static char argument_zero[] = "benchmark";
static char *arguments[] = {argument_zero, NULL};

void __builder_start(void) {
    for (char *byte = __bss_start; byte < __bss_end; ++byte) {
        *byte = 0;
    }
    for (char *byte = __tbss_start; byte < __tbss_end; ++byte) {
        *byte = 0;
    }
    builder_platform_init();
    for (init_function *function = __preinit_array_start;
         function < __preinit_array_end; ++function) {
        (*function)();
    }
    for (init_function *function = __init_array_start;
         function < __init_array_end; ++function) {
        (*function)();
    }
    builder_platform_finish(builder_main(1, arguments));
}
