#ifndef CONTAINER_IMAGES_BUILDER_SYS_RESOURCE_H
#define CONTAINER_IMAGES_BUILDER_SYS_RESOURCE_H
#define RUSAGE_SELF 0
struct rusage { long ru_utime; long ru_stime; };
static inline int getrusage(int who, struct rusage *usage) { (void)who; (void)usage; return 0; }
#endif
