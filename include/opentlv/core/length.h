// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_LENGTH_H
#define OPENTLV_LENGTH_H

#include <stddef.h>
#include <stdint.h>

/** @file
 * @brief Borrowed encoded length field.
 * @ingroup core
 */

/** Raw length bytes in wire order. Their numeric meaning comes from the
 * format. Every copy borrows the same bytes. Keep them alive and unchanged
 * until the last use. A null pointer and zero size denote an absent field.
 */
typedef struct {
    const uint8_t *data; /**< First encoded byte. May be null when size is zero. */
    size_t size;         /**< Number of bytes occupied by the field. */
} tlv_length_t;

#endif /* OPENTLV_LENGTH_H */
