// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_FUZZ_FORMATS_H
#define OPENTLV_FUZZ_FORMATS_H

#include "common.h"
#include "tlv/formats/fixed.h"
#include "tlv/builtins/bluetooth/bluetooth_ltv.h"
#include "tlv/builtins/asn1/ber.h"
#include "tlv/builtins/asn1/der.h"

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
 * call and cannot be compile-time constants like the other entries here. */
static const tlv_fixed_format_t fuzz_fixed_config = {
    {1}, {1, TLV_BYTE_ORDER_BIG_ENDIAN}, TLV_ELEMENT_ORDER_TLV, TLV_LENGTH_SCOPE_VALUE};
static tlv_format_t fuzz_fixed_format;
static tlv_format_t fuzz_bluetooth_ltv_format;

static const struct {
    const tlv_format_t* format;
} fuzz_formats[] = {{&fuzz_fixed_format},
                    {&fuzz_bluetooth_ltv_format},
                    {&tlv_format_ber},
                    {&tlv_format_der},
                    {NULL}};

/* libFuzzer calls this once before any input is fuzzed. */
int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;
    (void)argv;
    tlv_fixed_format_init(&fuzz_fixed_format, &fuzz_fixed_config);
    fuzz_fixed_format.is_constructed = fuzz_constructed;
    fuzz_bluetooth_ltv_format = tlv_format_bluetooth_ltv;
    fuzz_bluetooth_ltv_format.is_constructed = fuzz_constructed;
    return 0;
}

#endif
