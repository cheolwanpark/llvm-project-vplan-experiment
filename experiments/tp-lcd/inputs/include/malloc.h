#ifndef CONTAINER_IMAGES_BUILDER_MALLOC_H
#define CONTAINER_IMAGES_BUILDER_MALLOC_H
#include <stddef.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void *memalign(size_t alignment, size_t size);
#ifdef __cplusplus
}
#endif
#endif
