/*
 * Copyright (c) 2011 University of Illinois at Urbana-Champaign.
 * TSVC license: ../../benchmarks/TSVC-2/upstream/license.txt
 *
 * Small, freestanding adaptations of representative UoB TSVC-2 loops.
 * Original loop identifiers are kept in each kernel name.  Array sizes are
 * deliberately reduced for RTL simulation; loop bodies retain TSVC shape.
 */
#include <stdint.h>
#include <stdio.h>

#define LOOP_SIZE 4096
#define STRIDED_OUTPUT_SIZE (2 * LOOP_SIZE)
#define LEN_2D 64
#define REPEATS 32
#define ALIGNMENT 64

#ifndef TSVC_CASE
#error "TSVC_CASE must select one kernel"
#endif

static float a[STRIDED_OUTPUT_SIZE] __attribute__((aligned(ALIGNMENT)));
static float b[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float c[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float d[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static int index_array[LOOP_SIZE] __attribute__((aligned(ALIGNMENT)));
static float flat_2d_array[LEN_2D * LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float aa[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float bb[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static float cc[LEN_2D][LEN_2D] __attribute__((aligned(ALIGNMENT)));
static volatile float result_sink;

extern uint64_t builder_platform_cycle(void);

__attribute__((noinline, optnone))
static void initialise_arrays(void) {
    for (int i = 0; i < STRIDED_OUTPUT_SIZE; ++i)
        a[i] = 1.0f + (float)(i & 7) * 0.01f;
    for (int i = 0; i < LOOP_SIZE; ++i) {
        b[i] = 2.0f + (float)(i & 3) * 0.02f;
        c[i] = 3.0f + (float)(i & 15) * 0.01f;
        d[i] = 4.0f + (float)(i & 7) * 0.03f;
        index_array[i] = (i * 5) & (LOOP_SIZE - 1);
    }
    for (int i = 0; i < LEN_2D; ++i) {
        for (int j = 0; j < LEN_2D; ++j) {
            aa[i][j] = 1.0f + (float)i * 0.01f;
            bb[i][j] = 2.0f + (float)j * 0.01f;
            cc[i][j] = 0.5f + (float)((i + j) & 7) * 0.02f;
            flat_2d_array[i * LEN_2D + j] = 0.0f;
        }
    }
}

static inline uint64_t read_cycle(void) {
    __asm__ volatile("fence rw, rw" ::: "memory");
    uint64_t value = builder_platform_cycle();
    __asm__ volatile("fence rw, rw" ::: "memory");
    return value;
}

#if TSVC_CASE == 0
/* TSVC-2 s000: unit-stride map. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[i] = b[i] + 1.0f;
    return a[LOOP_SIZE - 1];
}
#elif TSVC_CASE == 1
/* TSVC-2 s1111: mixed arithmetic with a strided store. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[2 * i] = c[i] * b[i] + d[i] * b[i] + c[i] * c[i]
                 + d[i] * b[i] + d[i] * c[i];
    return a[STRIDED_OUTPUT_SIZE - 2];
}
#elif TSVC_CASE == 2
/* TSVC-2 s125: collapsed-index 2-D map, 64 x 64 = 4096 elements. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    float *aa_flat = &aa[0][0];
    float *bb_flat = &bb[0][0];
    float *cc_flat = &cc[0][0];
    for (int k = 0; k < LEN_2D * LEN_2D; ++k)
        flat_2d_array[k] = aa_flat[k] + bb_flat[k] * cc_flat[k];
    return flat_2d_array[LEN_2D * LEN_2D - 1];
}
#elif TSVC_CASE == 3
/* TSVC-2 s311: floating-point sum reduction. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    float sum = 0.0f;
    for (int i = 0; i < LOOP_SIZE; ++i)
        sum += a[i];
    return sum;
}
#elif TSVC_CASE == 4
/* TSVC-2 s4112: sparse SAXPY requiring a gather. */
__attribute__((noinline))
static float selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
    const float scale = 0.5f;
    for (int i = 0; i < LOOP_SIZE; ++i)
        a[i] += b[index_array[i]] * scale;
    return a[LOOP_SIZE - 1];
}
#else
#error "unknown TSVC_CASE"
#endif

int builder_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    initialise_arrays();

    float result = 0.0f;
    uint64_t begin = read_cycle();
    for (int repeat = 0; repeat < REPEATS; ++repeat)
        result += selected_kernel();
    uint64_t end = read_cycle();

    result_sink = result;
    printf("MB_ROI=%016llx\n", (unsigned long long)(end - begin));
    return result_sink == -1.0f;
}
