// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

/* Reuse the immutable release workloads; add the missing Value Codec workload. */
#define main reference_main
#include "workloads.c"
#undef main
#include "opentlv/codec/number.h"

int main(int argc, char** argv) {
    const tlv_number_codec_config_t config = {TLV_NUMBER_BINARY_BE, 8, 0};
    struct timespec                 start, finish;
    uint64_t                        total = 0, iterations, i;
    char*                           end;
    if (argc != 3 || strcmp(argv[1], "codec") != 0) return reference_main(argc, argv);
    errno = 0;
    iterations = strtoull(argv[2], &end, 10);
    if (errno || *end || iterations == 0 || iterations > 100000000) return 2;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) return 1;
    for (i = 0; i < iterations; ++i) {
        uint64_t j;
        for (j = 0; j < 256; ++j) {
            const uint64_t value = UINT64_C(0x0102030405060700) + j;
            uint64_t       decoded = 0;
            uint8_t        wire[8];
            size_t         written = 0;
            if (tlv_number_encode(&config, &value, sizeof value, wire, sizeof wire, &written,
                                  NULL) != TLV_OK ||
                written != sizeof wire ||
                tlv_number_decode(&config, wire, written, &decoded, sizeof decoded, NULL) !=
                    TLV_OK ||
                decoded != value)
                return 1;
            total += decoded;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &finish) != 0) return 1;
    printf("{\"workload\":\"codec\",\"iterations\":%" PRIu64 ",\"elapsed_ns\":%" PRIu64
           ",\"checksum\":%" PRIu64 ",\"input_bytes\":2048,\"input_id\":\"binary-be-256x8-v1\"}\n",
           iterations, elapsed_ns(&start, &finish), total);
    return 0;
}
