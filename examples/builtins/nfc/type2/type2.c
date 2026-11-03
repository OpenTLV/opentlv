// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/nfc/type2.h"
#include "opentlv/io/reader/reader.h"
#include "opentlv/io/writer/writer.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    /* A contiguous TLV stream from the data area, not a physical memory dump. */
    /* Same bytes as examples/builtins/nfc/type2/sample.bin: an English "Hi" Text record. */
    const uint8_t  sample[] = {0x00, 0x03, 0x09, 0xD1, 0x01, 0x05, 0x54,
                               0x02, 0x65, 0x6E, 0x48, 0x69, 0xFE};
    uint8_t        input[1024];
    const uint8_t* stream = sample;
    size_t         stream_size = sizeof(sample);
    if (argc > 2) return 1;
    if (argc == 2) {
        FILE* file = fopen(argv[1], "rb");
        if (!file) return 1;
        stream_size = fread(input, 1, sizeof(input), file);
        int extra = fgetc(file);
        int invalid = ferror(file) || extra != EOF || stream_size == 0;
        if (fclose(file) != 0) invalid = 1;
        if (invalid) return 1;
        stream = input;
    }
    uint8_t      encoded[sizeof(input)] = {0};
    tlv_reader_t reader;
    tlv_writer_t writer;
    if (tlv_reader_init(&reader, stream, stream_size, &tlv_format_nfc_type2) != TLV_OK ||
        tlv_writer_init(&writer, encoded, sizeof(encoded), &tlv_format_nfc_type2) != TLV_OK)
        return 1;
    while (!tlv_reader_at_end(&reader)) {
        tlv_element_t element;
        if (tlv_reader_next(&reader, &element) != TLV_OK) return 1;
        /* Preserve NULL in the output, even though the display hides it. */
        if (tlv_writer_copy_element(&writer, &element) != TLV_OK) return 1;
        if (element.tag.data[0] == TLV_NFC_TYPE2_NULL) continue;
        if (element.tag.data[0] == TLV_NFC_TYPE2_TERMINATOR) {
            puts("TERMINATOR");
            break; /* Termination policy belongs to the caller. */
        }
        if (element.tag.data[0] == TLV_NFC_TYPE2_NDEF_MESSAGE) {
            printf("NDEF_MESSAGE:");
            for (size_t i = 0; i < (size_t)element.value.size; ++i)
                printf(" %02X", (unsigned)element.value.data[i]);
            puts("");
        }
    }
    return tlv_writer_size(&writer) == stream_size && memcmp(stream, encoded, stream_size) == 0 ? 0
                                                                                                : 1;
}
