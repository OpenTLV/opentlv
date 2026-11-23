// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TAG_H
#define OPENTLV_TAG_H

#include <stddef.h>
#include <stdint.h>

/** @file
 * @brief Borrowed TLV identifier bytes.
 * @ingroup core
 */

/** A tag in wire order. Copying a tag shares its backing storage. The caller
 * must keep that storage alive and unchanged until the last use of any copy.
 * The descriptor permits an empty tag. Fixed-width tags require a positive width.
 */
typedef struct {
    const uint8_t *data; /**< First tag byte. May be null when size is zero. */
    size_t size;         /**< Number of readable identifier bytes. */
} tlv_tag_t;

#endif /* OPENTLV_TAG_H */
