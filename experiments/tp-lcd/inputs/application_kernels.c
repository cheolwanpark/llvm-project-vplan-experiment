/*
 * Freestanding, single-row adaptations of PolyBench/C 3.2 and ONNX Runtime.
 * PolyBench: Copyright (c) 2011-2012 the Ohio State University.
 * PolyBench authors: Louis-Noel Pouchet and Uday Bondugula.
 * PolyBench sources: ../../benchmarks/Polybench/upstream/ (see README.md).
 * RoPE: Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License; see licenses/onnxruntime-MIT.txt.
 * ORT revision: 1bc68c1d25b374e13bbf9a0649441af1ad9f1501.
 *
 * Each call executes one original inner loop, with no matrix allocation,
 * copy-back, dispatch, or other benchmark phases in the measured region.
 */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef APP_LOOP_SIZE
#error "APP_LOOP_SIZE must be supplied by suite.py"
#endif
#if APP_LOOP_SIZE <= 0 || APP_LOOP_SIZE % 2 != 0
#error "APP_LOOP_SIZE must be a positive even number"
#endif
#ifndef APP_CASE
#error "APP_CASE must select one kernel"
#endif
#ifndef APP_REPEATS
#error "APP_REPEATS must be supplied by suite.py"
#endif

#define ALIGNED __attribute__((aligned(64)))
static float a[APP_LOOP_SIZE + 2] ALIGNED;
static float b[APP_LOOP_SIZE + 2] ALIGNED;
static float c[APP_LOOP_SIZE + 2] ALIGNED;
static float x[APP_LOOP_SIZE] ALIGNED;
static float output[APP_LOOP_SIZE] ALIGNED;
static float scalars[2] ALIGNED;
static volatile float result_sink;

__attribute__((noinline, optnone))
static void initialise_arrays(void) {
    for (int i = 0; i < APP_LOOP_SIZE + 2; ++i) {
        a[i] = 0.25f + (float)(i % 17) * 0.03125f;
        b[i] = -0.5f + (float)(i % 13) * 0.0625f;
        c[i] = 0.125f + (float)(i % 19) * 0.015625f;
    }
    for (int i = 0; i < APP_LOOP_SIZE; ++i) {
        x[i] = -0.25f + (float)(i % 11) * 0.0625f;
        output[i] = 0.125f + (float)(i % 7) * 0.03125f;
    }
#if APP_CASE == 3
    for (int i = 0; i < APP_LOOP_SIZE / 2; ++i) {
        b[i] = sinf((float)i * 0.01f);
        c[i] = cosf((float)i * 0.01f);
    }
#endif
    scalars[0] = scalars[1] = 0.0f;
}

__attribute__((noinline))
static void selected_kernel(void) {
    __asm__ volatile("" ::: "memory");
#if APP_CASE == 0
    /* Jacobi: a = middle row, b = upper row, c = lower row; both halos kept. */
    /* Target: jacobi-2d-imper-l00 */
    for (int j = 1; j <= APP_LOOP_SIZE; ++j)
        output[j - 1] = 0.2f * (a[j] + a[j - 1] + a[j + 1] + c[j] + b[j]);
#elif APP_CASE == 1
    /* GESUMMV: a/b = A/B row, x shared by both dot products. */
    float tmp = 0.0f;
    float y = 0.0f;
    /* Target: gesummv-l00 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j) {
        tmp = a[j] * x[j] + tmp;
        y = b[j] * x[j] + y;
    }
    scalars[0] = tmp;
    scalars[1] = 1.5f * tmp + 1.2f * y;
#elif APP_CASE == 2
    /* BiCG: a = A row, x = p, output = s; r[i] = 0.75. */
    float q = 0.0f;
    /* Target: bicg-l01 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j) {
        output[j] = output[j] + 0.75f * a[j];
        q = q + a[j] * x[j];
    }
    scalars[0] = q;
#elif APP_CASE == 3
    /* ORT MlasRotaryEmbedOneRow_FallBack<float>, interleaved = false.
     * a = input, b = sin_data, c = cos_data. Preserve the scalar indexing.
     */
    const size_t half = APP_LOOP_SIZE / 2;
    /* Target: rope-f32-noninterleaved */
    for (size_t i = 0; i < APP_LOOP_SIZE; ++i) {
        size_t cache_idx = i % half;
        bool sign = i >= half;
        size_t j = (i + half) % APP_LOOP_SIZE;
        float value = a[i] * c[cache_idx];
        if (sign)
            value += a[j] * b[cache_idx];
        else
            value -= a[j] * b[cache_idx];
        output[i] = value;
    }
#elif APP_CASE == 4
    /* GEMVER: output = A row, b/c = v1/v2; u1[i]/u2[i] = 0.5/0.75. */
    /* Target: gemver-l00 */
    for (int j = 0; j < APP_LOOP_SIZE; ++j)
        output[j] = output[j] + 0.5f * b[j] + 0.75f * c[j];
#else
#error "unknown APP_CASE"
#endif
}

/* Consume every output outside the ROI, without creating vector candidates. */
__attribute__((noinline, optnone))
static void consume_outputs(void) {
    float sum = scalars[0] + scalars[1];
    for (int i = 0; i < APP_LOOP_SIZE; ++i)
        sum += output[i];
    result_sink = sum;
}

extern uint64_t builder_platform_cycle(void);

static inline uint64_t read_cycle(void) {
#ifdef __riscv
    __asm__ volatile("fence rw, rw" ::: "memory");
#endif
    uint64_t value = builder_platform_cycle();
#ifdef __riscv
    __asm__ volatile("fence rw, rw" ::: "memory");
#endif
    return value;
}

int builder_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    initialise_arrays();
    uint64_t begin = read_cycle();
#pragma clang loop vectorize(disable) unroll(disable)
    for (int repeat = 0; repeat < APP_REPEATS; ++repeat)
        selected_kernel();
    uint64_t end = read_cycle();
    consume_outputs();
    printf("MB_ROI=%016llx\n", (unsigned long long)(end - begin));
    return result_sink == -1.0f;
}
