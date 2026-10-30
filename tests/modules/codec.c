// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/codec/values.h"
#include "tlv/codec/number.h"
#include "tlv/codec/digits.h"
#include "tlv/codec/text.h"
#include "tlv/codec/ipv4.h"
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) return __LINE__;                                                                 \
    } while (0)

int main(void) {
    uint8_t                wire[8] = {0};
    size_t                 written = 0;
    uint16_t               value = 0x1234, decoded = 0;
    tlv_codec_diagnostic_t diagnostic;
    CHECK(tlv_codec_encode(&tlv_codec_uint16_be, &value, sizeof value, NULL, 0, &written,
                           &diagnostic) == TLV_OK &&
          written == 2);
    CHECK(diagnostic.codec.operation == TLV_CODEC_OP_MEASURE);
    CHECK(tlv_codec_encode(&tlv_codec_uint16_be, &value, sizeof value, wire, sizeof wire, &written,
                           NULL) == TLV_OK);
    CHECK(wire[0] == 0x12 && wire[1] == 0x34);
    CHECK(tlv_codec_decode(&tlv_codec_uint16_be, wire, written, &decoded, sizeof decoded, NULL) ==
              TLV_OK &&
          decoded == value);
    CHECK(tlv_codec_decode(&tlv_codec_uint16_be, wire, 1, &decoded, sizeof decoded, &diagnostic) ==
          TLV_ERR_INVALID_VALUE);
    CHECK(decoded == value && diagnostic.codec.reason == TLV_CODEC_REASON_LENGTH);
    CHECK(diagnostic.causes.count == 0);
    tlv_diagnostic_envelope_t envelope;
    tlv_codec_diagnostic_export(&envelope, &diagnostic);
    CHECK(envelope.causes.count == 1);
    CHECK(envelope.causes.frames[0].domain->id == 6);
    CHECK(strcmp(envelope.causes.frames[0].domain->code_name(4, TLV_CODEC_REASON_LENGTH),
                 "length") == 0);
    tlv_codec_detail_t detail;
    CHECK(tlv_codec_detail_copy(&envelope.causes.frames[0], &detail) == TLV_OK);
    CHECK(detail.operation == TLV_CODEC_OP_DECODE && detail.reason == TLV_CODEC_REASON_LENGTH);

    const tlv_number_codec_config_t number_config = {TLV_NUMBER_BCD, 2, 4};
    const tlv_codec_t               number_codec = tlv_number_codec(&number_config);
    uint64_t                        number = 1234, restored = 0;
    CHECK(tlv_codec_encode(&number_codec, &number, sizeof number, wire, sizeof wire, &written,
                           NULL) == TLV_OK);
    CHECK(written == 2 && wire[0] == 0x12 && wire[1] == 0x34);
    CHECK(tlv_codec_decode(&number_codec, wire, written, &restored, sizeof restored, NULL) ==
              TLV_OK &&
          restored == number);
    const tlv_digits_codec_config_t digits = {0};
    char                            output[8];
    CHECK(tlv_digits_decode(&digits, wire, written, output, sizeof output, NULL) == TLV_OK);
    CHECK(strcmp(output, "1234") == 0);
    const tlv_text_codec_config_t text = {TLV_TEXT_ASCII_ALNUM, 0, 0};
    tlv_value_t                   span;
    CHECK(tlv_text_decode(&text, (const uint8_t*)output, 4, &span, sizeof span, NULL) == TLV_OK);
    CHECK(span.data == (const uint8_t*)output && span.size == 4);
    const uint8_t address[] = {192, 0, 2, 1};
    tlv_ipv4_t    ipv4;
    CHECK(tlv_codec_decode(&tlv_codec_ipv4, address, sizeof address, &ipv4, sizeof ipv4, NULL) ==
          TLV_OK);
    CHECK(memcmp(ipv4.bytes, address, sizeof address) == 0);
    CHECK(tlv_codec_decode(&tlv_codec_bytes, address, sizeof address, &span, sizeof span, NULL) ==
              TLV_OK &&
          span.data == address);
    return 0;
}
