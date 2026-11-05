// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
#ifdef OPENTLV_FUZZ_INITIALIZE
int LLVMFuzzerInitialize(int* argc, char*** argv);
#endif

int main(int argc, char** argv) {
#ifdef OPENTLV_FUZZ_INITIALIZE
    if (LLVMFuzzerInitialize(&argc, &argv) != 0) {
        return 1;
    }
#endif
    fprintf(stderr, "replay: empty input\n");
    const uint8_t empty = 0;
    if (LLVMFuzzerTestOneInput(&empty, 0) != 0) {
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        fprintf(stderr, "replay: %s\n", argv[i]);
        FILE* file = fopen(argv[i], "rb");
        if (!file) {
            return 2;
        }
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return 2;
        }
        long length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            return 2;
        }
        uint8_t* data = (uint8_t*)malloc((size_t)length + 1);
        if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
            free(data);
            fclose(file);
            return 2;
        }
        fclose(file);
        int result = LLVMFuzzerTestOneInput(data, (size_t)length);
        free(data);
        if (result != 0) {
            return 1;
        }
    }
    return 0;
}
