// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_BENCHMARK_ADAPTER_H
#define OPENTLV_BENCHMARK_ADAPTER_H

#include <opentlv/tlv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bench_operation {
    BENCH_FIELD_READ,
    BENCH_FIELD_WRITE,
    BENCH_HEADER_SIZE,
    BENCH_ELEMENT_SIZE,
    BENCH_READ,
    BENCH_NEXT,
    BENCH_WRITE,
    BENCH_APPEND
} bench_operation;

typedef struct bench_input {
    tlv_fixed_format_t format;
    tlv_element_t source;
    const uint8_t *data;
    size_t available;
    tlv_size_t length;
    int diagnostic;
} bench_input;

/* The caller seeds each output so validation can check preservation on error. */
typedef struct bench_output {
    tlv_result_t result;
    size_t count;
    tlv_size_t length;
    tlv_element_t view;
    size_t position;
    tlv_diagnostic_t diagnostic;
} bench_output;

void bench_run_c(bench_operation op, const bench_input *in, uint8_t *destination, bench_output *out);
void bench_run_cpp(bench_operation op, const bench_input *in, uint8_t *destination, bench_output *out);

#ifdef __cplusplus
}
#endif

#endif /* OPENTLV_BENCHMARK_ADAPTER_H */
