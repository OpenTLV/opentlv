// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "common.h"
#include "../../tlv/src/utf8_internal.h"

/* The validator, its streaming form and the diagnostic locator are separate
 * implementations of the same UTF-8 definition; they must never disagree. */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    size_t            begin = SIZE_MAX, end = SIZE_MAX, split = size ? data[0] % (size + 1) : 0;
    tlv_utf8_stream_t stream;
    tlv_result_t      valid = tlv_utf8_validate(data, size), streamed;
    int               found = tlv_utf8_find_invalid(data, size, &begin, &end);
    FUZZ_CHECK(valid == TLV_OK || valid == TLV_ERR_INVALID_VALUE);
    FUZZ_CHECK(found == (valid != TLV_OK));
    if (found) {
        /* A nonempty maximal subpart inside the input, preceded by well-formed text. */
        FUZZ_CHECK(begin < end && end <= size);
        FUZZ_CHECK(tlv_utf8_validate(data, begin) == TLV_OK);
        FUZZ_CHECK(end - begin <= 3);
    } else {
        FUZZ_CHECK(begin == SIZE_MAX && end == SIZE_MAX);
    }
    /* A split anywhere, as between CER segments, does not change the answer. */
    tlv_utf8_stream_init(&stream);
    streamed = tlv_utf8_stream_update(&stream, data, split);
    if (streamed == TLV_OK) streamed = tlv_utf8_stream_update(&stream, data + split, size - split);
    if (streamed == TLV_OK) streamed = tlv_utf8_stream_finish(&stream);
    FUZZ_CHECK(streamed == valid);
    return 0;
}
