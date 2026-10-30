// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/generator.h"
#include "tlv/formats/fixed.h"
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) return __LINE__;                                                         \
    } while (0)

int main(void) {
    const uint8_t                   tag = 1;
    const tlv_generator_candidate_t candidate = {{&tag, 1}, 0, 16};
    const tlv_fixed_format_t        config = {
        {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
    tlv_generator_options_t options = {42, 7, 8, 0, 16, 128, &candidate, 1};
    tlv_format_t            format;
    uint8_t                 output[128], repeated[256], workspace[128];
    size_t                  required = 0, written = 0, again = 0, offset = 0;
    CHECK(TLV_GENERATOR_VERSION == 2);
    CHECK(tlv_fixed_format_init(&format, &config) == TLV_OK);
    CHECK(tlv_generator_workspace_size(&options, &required) == TLV_OK);
    CHECK(required == sizeof(workspace));
    CHECK(tlv_generate(&format, &options, output, sizeof(output), workspace, sizeof(workspace),
                       &written) == TLV_OK);
    CHECK(written > 0 && written <= sizeof(output));
    CHECK(tlv_generate(&format, &options, repeated, sizeof(repeated), workspace, sizeof(workspace),
                       &again) == TLV_OK);
    CHECK(written == again && memcmp(output, repeated, written) == 0);
    while (offset < written) {
        tlv_decoded_t decoded;
        CHECK(tlv_format_decode(&format, output + offset, written - offset, &decoded, NULL) ==
              TLV_OK);
        CHECK(decoded.source.size > 0 && decoded.source.size <= written - offset);
        CHECK(decoded.element.tag.size == 1 && decoded.element.tag.data[0] == tag);
        offset += (size_t)decoded.source.size;
    }
    again = 999;
    CHECK(tlv_generate(&format, &options, repeated, sizeof(repeated), workspace, required - 1,
                       &again) == TLV_ERR_BUFFER_TOO_SHORT);
    CHECK(again == 999);
    options.max_depth = 65;
    CHECK(tlv_generator_workspace_size(&options, &required) == TLV_ERR_UNSUPPORTED);
    CHECK(required == sizeof(workspace));
    return 0;
}
