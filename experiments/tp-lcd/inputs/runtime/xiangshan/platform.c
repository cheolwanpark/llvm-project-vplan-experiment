#include <stdint.h>

#define UART_TX (*(volatile unsigned char *)0x40600004UL)
static uint64_t run_start_cycle;

uint64_t builder_platform_cycle(void) {
    uint64_t value;
    __asm__ volatile("rdcycle %0" : "=r"(value));
    return value;
}
void builder_platform_init(void) { run_start_cycle = builder_platform_cycle(); }
long builder_platform_write(const void *buffer, unsigned long count) {
    const unsigned char *bytes = buffer;
    for (unsigned long index = 0; index < count; ++index) UART_TX = bytes[index];
    return (long)count;
}
static void write_text(const char *text) {
    while (*text) UART_TX = (unsigned char)*text++;
}
static void write_decimal(uint64_t value) {
    char buffer[32];
    unsigned int position = sizeof(buffer);
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    builder_platform_write(buffer + position, sizeof(buffer) - position);
}
void builder_platform_finish(int code) {
    write_text("KC=");
    write_decimal(builder_platform_cycle() - run_start_cycle);
    write_text(code == 0 ? "\nPASSED\n" : "\nFAILED\n");
    register uint64_t status __asm__("a0") = (uint64_t)(unsigned int)code;
    __asm__ volatile(".word 0x0000006b" : : "r"(status));
    for (;;) {}
}
