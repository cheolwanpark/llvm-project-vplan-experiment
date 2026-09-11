#include <cstddef>
#include <cstdlib>
#include <new>

extern "C" void *memalign(std::size_t alignment, std::size_t size);

#define BUILDER_WEAK __attribute__((weak))

[[noreturn]] static void allocation_failed() { std::abort(); }

BUILDER_WEAK void *operator new(std::size_t size) {
    if (void *pointer = std::malloc(size)) return pointer;
    allocation_failed();
}
BUILDER_WEAK void *operator new[](std::size_t size) { return ::operator new(size); }
BUILDER_WEAK void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return std::malloc(size);
}
BUILDER_WEAK void *operator new[](std::size_t size, const std::nothrow_t &value) noexcept {
    return ::operator new(size, value);
}
BUILDER_WEAK void *operator new(std::size_t size, std::align_val_t alignment) {
    if (void *pointer = memalign(static_cast<std::size_t>(alignment), size)) return pointer;
    allocation_failed();
}
BUILDER_WEAK void *operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
BUILDER_WEAK void *operator new(
    std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return memalign(static_cast<std::size_t>(alignment), size);
}
BUILDER_WEAK void *operator new[](
    std::size_t size, std::align_val_t alignment, const std::nothrow_t &value) noexcept {
    return ::operator new(size, alignment, value);
}
BUILDER_WEAK void operator delete(void *pointer) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer, std::size_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::align_val_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete[](void *pointer, std::align_val_t) noexcept { std::free(pointer); }
BUILDER_WEAK void operator delete(void *pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}
BUILDER_WEAK void operator delete[](void *pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

extern "C" {
BUILDER_WEAK void *__dso_handle = &__dso_handle;
BUILDER_WEAK int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
BUILDER_WEAK int __cxa_thread_atexit(void (*)(void *), void *, void *) { return 0; }
BUILDER_WEAK int __cxa_guard_acquire(unsigned long long *guard) {
    return *reinterpret_cast<unsigned char *>(guard) == 0;
}
BUILDER_WEAK void __cxa_guard_release(unsigned long long *guard) {
    *reinterpret_cast<unsigned char *>(guard) = 1;
}
BUILDER_WEAK void __cxa_guard_abort(unsigned long long *) {}
BUILDER_WEAK void __cxa_finalize(void *) {}
BUILDER_WEAK void __cxa_pure_virtual(void) { allocation_failed(); }
}

namespace std {
inline namespace __1 {
[[noreturn]] BUILDER_WEAK void __libcpp_verbose_abort(char const *, ...) noexcept {
    allocation_failed();
}
[[noreturn]] BUILDER_WEAK void terminate() noexcept { allocation_failed(); }
}
}
