#ifndef CONTAINER_IMAGES_BUILDER_SCHED_H
#define CONTAINER_IMAGES_BUILDER_SCHED_H
#define SCHED_OTHER 0
#define SCHED_FIFO 1
struct sched_param { int sched_priority; };
static inline int sched_get_priority_max(int policy) { (void)policy; return 0; }
static inline int sched_setscheduler(int pid, int policy, const struct sched_param *parameter) {
    (void)pid; (void)policy; (void)parameter; return 0;
}
#endif
