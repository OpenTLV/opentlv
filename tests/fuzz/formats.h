// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_FUZZ_FORMATS_H
#define OPENTLV_FUZZ_FORMATS_H

#include "common.h"
#include "opentlv/core/formats/fixed.h"
#include "opentlv/builtin/bluetooth/bluetooth_ltv.h"
#include "opentlv/builtin/asn1/ber.h"
#include "opentlv/builtin/asn1/der.h"

/* Raw-byte formats have no built-in nesting rule. This test convention lets
 * the Tree Reader exercise their nested values too: bit 0x20 denotes a container. */
static inline int fuzz_constructed(const void* context, const tlv_tag_t* tag) {
    (void)context;
    return (tag->data[0] & 0x20) != 0;
}

/* Local, mutable copies of the formats without a built-in nesting rule, so
 * fuzz_constructed() can be attached to `is_constructed` without mutating the
 * library's own extern const globals. Populated by LLVMFuzzerInitialize()
 * below; tlv_fixed_format_t-based formats additionally need a runtime init
 * call. */
static const tlv_fixed_format_t fuzz_fixed_config = {
    {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
static tlv_format_t fuzz_fixed_format;
static tlv_format_t fuzz_bluetooth_ltv_format;

/* NULL-terminated; filled by LLVMFuzzerInitialize(). The address of a format
 * imported from a shared MSVC module is not a C constant expression. */
static struct {
    const tlv_format_t* format;
} fuzz_formats[5];

/* libFuzzer calls this once before any input is fuzzed. */
int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;
    (void)argv;
    tlv_fixed_format_init(&fuzz_fixed_format, &fuzz_fixed_config);
    fuzz_fixed_format.is_constructed = fuzz_constructed;
    fuzz_bluetooth_ltv_format = tlv_format_bluetooth_ltv;
    fuzz_bluetooth_ltv_format.is_constructed = fuzz_constructed;
    fuzz_formats[0].format = &fuzz_fixed_format;
    fuzz_formats[1].format = &fuzz_bluetooth_ltv_format;
    fuzz_formats[2].format = &tlv_format_ber;
    fuzz_formats[3].format = &tlv_format_der;
    return 0;
}

#endif
