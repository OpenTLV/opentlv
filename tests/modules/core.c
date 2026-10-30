// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/callback.h"
#include "tlv/copy.h"
#include "tlv/diagnostic/cause.h"
#include "tlv/formats/fixed.h"
#include "tlv/utf8.h"
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) return __LINE__;                                                         \
    } while (0)

int main(void) {
    static const tlv_diagnostic_domain_t domain = {0x8000, "test.producer", NULL, NULL};
    static const tlv_diagnostic_domain_t foreign = {0x8000, "test.foreign", NULL, NULL};
    const uint8_t                        valid[] = {0xF0, 0x9F, 0x98, 0x80};
    const uint8_t                        invalid[] = {0xED, 0xA0, 0x80};
    tlv_utf8_stream_t                    state;
    tlv_diagnostic_causes_t              causes, copy;
    int                                  detail = 42, result = 0;
    size_t                               begin = 99, end = 99;
    CHECK(tlv_callback_result(TLV_END, 0) == TLV_ERR_CALLBACK);
    CHECK(tlv_callback_result(TLV_END, 1) == TLV_END);
    CHECK(tlv_callback_result(TLV_ERR_LIMIT, 0) == TLV_ERR_LIMIT);
    CHECK(tlv_callback_result((tlv_result_t)999, 0) == TLV_ERR_CALLBACK);
    CHECK(tlv_utf8_validate(valid, sizeof valid) == TLV_OK);
    CHECK(tlv_utf8_validate(invalid, sizeof invalid) == TLV_ERR_INVALID_VALUE);
    CHECK(tlv_utf8_find_invalid(invalid, sizeof invalid, &begin, &end));
    CHECK(begin == 0 && end == 1);
    tlv_utf8_stream_init(&state);
    CHECK(tlv_utf8_stream_update(&state, valid, 2) == TLV_OK);
    CHECK(tlv_utf8_stream_finish(&state) == TLV_ERR_INVALID_VALUE);
    CHECK(tlv_utf8_stream_update(&state, valid + 2, 2) == TLV_OK);
    CHECK(tlv_utf8_stream_finish(&state) == TLV_OK);
    tlv_diagnostic_causes_init(&causes);
    CHECK(tlv_diagnostic_causes_append(&causes, &domain, 1, &detail, sizeof detail) == TLV_OK);
    copy = causes;
    tlv_diagnostic_causes_init(&causes);
    CHECK(tlv_diagnostic_cause_copy(&copy.frames[0], &domain, 1, &result, sizeof result) == TLV_OK);
    CHECK(result == 42);
    CHECK(tlv_diagnostic_cause_copy(&copy.frames[0], &foreign, 1, &result, sizeof result) ==
          TLV_ERR_INVALID_ARG);
    CHECK(result == 42);
    CHECK(tlv_diagnostic_causes_append(&copy, &domain, 1, &detail, TLV_DIAGNOSTIC_DETAIL_MAX + 1) ==
          TLV_ERR_INVALID_ARG);
    CHECK(copy.count == 1);
    CHECK(tlv_diagnostic_causes_append(&copy, &domain, 2, &detail, sizeof detail) == TLV_OK);
    CHECK(tlv_diagnostic_causes_append(&copy, &domain, 3, &detail, sizeof detail) ==
          TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(copy.count == 2 && copy.omitted == 1);
    copy.omitted = SIZE_MAX;
    CHECK(tlv_diagnostic_causes_append(&copy, &domain, 3, &detail, sizeof detail) ==
          TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(copy.omitted == SIZE_MAX);
    {
        tlv_fixed_format_t config = {
            {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
        tlv_format_t format;
        uint8_t      data[4] = {1, 2, 3, 4};
        size_t       written = 0;
        CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
        CHECK(tlv_format_can_read(&format) && tlv_format_can_write(&format));
        CHECK(tlv_copy_encoded(data, 3, data + 1, 3, &written) == TLV_OK);
        CHECK(written == 3 && data[1] == 1 && data[3] == 3);
    }
    return 0;
}
