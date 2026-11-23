// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_ELEMENT_H
#define OPENTLV_ELEMENT_H

#include "opentlv/core/tag.h"
#include "opentlv/core/value.h"

/** @file
 * @brief Format-independent TLV element.
 * @ingroup core
 */

/** The identifier and its decoded value. Both borrow storage from the caller.
 * Keep the backing bytes alive and unchanged while any copy is in use.
 */
typedef struct {
    tlv_tag_t tag;     /**< Identifier in wire order. */
    tlv_value_t value; /**< Payload bytes and their logical count. */
} tlv_element_t;

#endif /* OPENTLV_ELEMENT_H */
