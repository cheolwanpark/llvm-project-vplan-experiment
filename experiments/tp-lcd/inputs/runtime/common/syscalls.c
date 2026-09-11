#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>

extern char __heap_start[], __heap_end[];
extern long builder_platform_write(const void *buffer, unsigned long count);
extern uint64_t builder_platform_cycle(void);
extern void builder_platform_finish(int code) __attribute__((noreturn));

struct _reent;
static char *heap_current = __heap_start;

void *_sbrk(ptrdiff_t increment) {
    char *previous = heap_current;
    char *next = heap_current + increment;
    if (next < __heap_start || next > __heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_current = next;
    return previous;
}

void *sbrk(ptrdiff_t increment) { return _sbrk(increment); }
void *_sbrk_r(struct _reent *reent, ptrdiff_t increment) {
    (void)reent;
    return _sbrk(increment);
}

long _write(int fd, const void *buffer, unsigned long count) {
    (void)fd;
    return builder_platform_write(buffer, count);
}
long write(int fd, const void *buffer, unsigned long count) { return _write(fd, buffer, count); }
long _write_r(struct _reent *reent, int fd, const void *buffer, unsigned long count) {
    (void)reent;
    return _write(fd, buffer, count);
}

long _read(int fd, void *buffer, unsigned long count) {
    (void)fd; (void)buffer; (void)count;
    return 0;
}
long read(int fd, void *buffer, unsigned long count) { return _read(fd, buffer, count); }
long _read_r(struct _reent *reent, int fd, void *buffer, unsigned long count) {
    (void)reent;
    return _read(fd, buffer, count);
}

int _close(int fd) { (void)fd; return 0; }
int close(int fd) { return _close(fd); }
int _close_r(struct _reent *reent, int fd) { (void)reent; return _close(fd); }

int _fstat(int fd, struct stat *status) {
    (void)fd;
    if (status) status->st_mode = S_IFCHR;
    return 0;
}
int fstat(int fd, struct stat *status) { return _fstat(fd, status); }
int _fstat_r(struct _reent *reent, int fd, struct stat *status) {
    (void)reent;
    return _fstat(fd, status);
}

int _isatty(int fd) { (void)fd; return 1; }
int isatty(int fd) { return _isatty(fd); }
int _isatty_r(struct _reent *reent, int fd) { (void)reent; return _isatty(fd); }

long _lseek(int fd, long offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return 0;
}
long lseek(int fd, long offset, int whence) { return _lseek(fd, offset, whence); }
long _lseek_r(struct _reent *reent, int fd, long offset, int whence) {
    (void)reent;
    return _lseek(fd, offset, whence);
}

int _getpid(void) { return 1; }
int getpid(void) { return _getpid(); }
int _getpid_r(struct _reent *reent) { (void)reent; return _getpid(); }
int _kill(int pid, int signal) { (void)pid; (void)signal; errno = EINVAL; return -1; }
int kill(int pid, int signal) { return _kill(pid, signal); }
int _kill_r(struct _reent *reent, int pid, int signal) {
    (void)reent;
    return _kill(pid, signal);
}

int gettimeofday(struct timeval *value, void *zone) {
    (void)zone;
    uint64_t cycles = builder_platform_cycle();
    if (value) {
        value->tv_sec = (long)(cycles / 1000000ULL);
        value->tv_usec = (long)(cycles % 1000000ULL);
    }
    return 0;
}
int _gettimeofday(struct timeval *value, void *zone) { return gettimeofday(value, zone); }
int _gettimeofday_r(struct _reent *reent, struct timeval *value, void *zone) {
    (void)reent;
    return gettimeofday(value, zone);
}

clock_t clock(void) { return (clock_t)builder_platform_cycle(); }
time_t time(time_t *result) {
    time_t value = (time_t)(builder_platform_cycle() / 1000000ULL);
    if (result) *result = value;
    return value;
}
clock_t times(struct tms *value) {
    clock_t now = clock();
    if (value) {
        value->tms_utime = now;
        value->tms_stime = value->tms_cutime = value->tms_cstime = 0;
    }
    return now;
}

void _exit(int code) { builder_platform_finish(code); }
void exit(int code) { builder_platform_finish(code); }
void abort(void) { builder_platform_finish(134); }
