// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_ATTRIBUTES_H
#define OPENTLV_ATTRIBUTES_H

#include "opentlv/core/compiler.h"

/**
 * @file
 * @ingroup core
 * @brief Portable declaration attributes built on opentlv/core/compiler.h.
 *
 * OpenTLV supplies TLV_NODISCARD, TLV_MAYBE_UNUSED and TLV_FALLTHROUGH.
 * Defining any of them before the first inclusion of this header is an error.
 * Each expands to a standard attribute in C23 and C++17 or later, a compiler
 * attribute in earlier modes when supported, or an empty replacement otherwise.
 */

/** @addtogroup core
 * @{
 */

#if defined(TLV_NODISCARD) || defined(TLV_MAYBE_UNUSED) || defined(TLV_FALLTHROUGH)
#error "OpenTLV reserves TLV_NODISCARD, TLV_MAYBE_UNUSED and TLV_FALLTHROUGH"
#endif

/* MSVC reports the selected C++ standard in _MSVC_LANG even when
 * __cplusplus retains its default value. */
#if TLV_HAS_C23 || (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define TLV_ATTRIBUTES_HAS_STD_ATTR 1
#else
#define TLV_ATTRIBUTES_HAS_STD_ATTR 0
#endif

/**
 * @brief Marks a function whose return value must not be silently discarded.
 *
 * Expands to the standard `[[nodiscard]]` attribute in C++17 or newer or in
 * C23, to `__attribute__((warn_unused_result))` on compilers that recognize
 * it (GCC, Clang), and to nothing otherwise.
 */
#if TLV_ATTRIBUTES_HAS_STD_ATTR
#define TLV_NODISCARD [[nodiscard]]
#elif TLV_HAS_ATTRIBUTE(warn_unused_result)
#define TLV_NODISCARD __attribute__((warn_unused_result))
#else
#define TLV_NODISCARD
#endif

/**
 * @brief Marks a declaration that may be intentionally unused.
 *
 * Expands to the standard `[[maybe_unused]]` attribute in C++17 or newer or
 * in C23, to `__attribute__((unused))` on compilers that recognize it (GCC,
 * Clang), and to nothing otherwise, including the C99 baseline.
 */
#if TLV_ATTRIBUTES_HAS_STD_ATTR
#define TLV_MAYBE_UNUSED [[maybe_unused]]
#elif TLV_HAS_ATTRIBUTE(unused)
#define TLV_MAYBE_UNUSED __attribute__((unused))
#else
#define TLV_MAYBE_UNUSED
#endif

/**
 * @brief Marks an intentional fall-through between adjacent `switch` cases.
 *
 * Used as a standalone statement, `TLV_FALLTHROUGH;`, as the last statement
 * of a `case` that intentionally falls into the next one. Expands to the
 * standard `[[fallthrough]]` attribute in C++17 or newer or in C23, to
 * `__attribute__((fallthrough))` on compilers that recognize it (GCC,
 * Clang), and to nothing otherwise, including the C99 baseline, where the
 * caller-supplied `;` alone forms a valid empty statement.
 */
#if TLV_ATTRIBUTES_HAS_STD_ATTR
#define TLV_FALLTHROUGH [[fallthrough]]
#elif TLV_HAS_ATTRIBUTE(fallthrough)
#define TLV_FALLTHROUGH __attribute__((fallthrough))
#else
#define TLV_FALLTHROUGH
#endif

#undef TLV_ATTRIBUTES_HAS_STD_ATTR

/** @} */

#endif /* OPENTLV_ATTRIBUTES_H */
