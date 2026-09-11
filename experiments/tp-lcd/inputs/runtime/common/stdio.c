#include <stdio.h>

extern long builder_platform_write(const void *buffer, unsigned long count);

static int builder_stdio_put(char character, FILE *stream) {
    (void)stream;
    return builder_platform_write(&character, 1) == 1 ? 0 : EOF;
}

static int builder_stdio_get(FILE *stream) {
    (void)stream;
    return EOF;
}

static int builder_stdio_flush(FILE *stream) {
    (void)stream;
    return 0;
}

static FILE builder_stdin = FDEV_SETUP_STREAM(
    NULL, builder_stdio_get, builder_stdio_flush, _FDEV_SETUP_READ);
static FILE builder_stdout = FDEV_SETUP_STREAM(
    builder_stdio_put, NULL, builder_stdio_flush, _FDEV_SETUP_WRITE);
static FILE builder_stderr = FDEV_SETUP_STREAM(
    builder_stdio_put, NULL, builder_stdio_flush, _FDEV_SETUP_WRITE);

FILE *const stdin = &builder_stdin;
FILE *const stdout = &builder_stdout;
FILE *const stderr = &builder_stderr;
