// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_DIAGNOSTIC_CAUSE_H
#define OPENTLV_DIAGNOSTIC_CAUSE_H

#include "tlv/diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * @brief Allocation-free transport for module-owned diagnostic detail.
 */

/** @brief Maximum retained delegated frames, outermost first. */
#define TLV_DIAGNOSTIC_CAUSES_MAX 2
/** @brief Maximum copied leaf detail size; producers must check their types fit. */
#define TLV_DIAGNOSTIC_DETAIL_MAX 160

/** @brief Static producer descriptor; it and its strings must outlive every frame.
 * IDs 1..255 identify generic modules, 256..511 builtins, 512..32767 are
 * reserved, and 32768..65535 identify external producers. External identity
 * uses both id and name. Zero is invalid. Names and callbacks are borrowed;
 * optional lookups return static strings and must tolerate unknown IDs.
 */
typedef struct tlv_diagnostic_domain {
    uint16_t id;                                         /**< Registered domain identifier. */
    const char* name;                                    /**< Stable nonempty producer name. */
    const char* (*detail_name)(uint16_t type);           /**< Optional detail type lookup. */
    const char* (*code_name)(uint16_t family, int code); /**< Optional code lookup. */
} tlv_diagnostic_domain_t;

/** @brief Inline payload storage. Use copy-out access, never cast bytes to a type. */
typedef union tlv_diagnostic_payload {
    uint8_t bytes[TLV_DIAGNOSTIC_DETAIL_MAX]; /**< Copied representation. */
    uint64_t integer_alignment;               /**< Integer alignment. */
    long double real_alignment;               /**< Fundamental floating-point alignment. */
    void* pointer_alignment;                  /**< Pointer alignment. */
} tlv_diagnostic_payload_t;

/** @brief One producer-owned leaf detail; embedded pointers keep their borrowing rules. */
typedef struct tlv_diagnostic_cause {
    const tlv_diagnostic_domain_t* domain; /**< Borrowed static descriptor. */
    uint16_t type;                         /**< Nonzero detail type within the domain. */
    size_t size;                           /**< Number of valid payload bytes. */
    tlv_diagnostic_payload_t payload;      /**< Value-copied leaf detail. */
} tlv_diagnostic_cause_t;

/** @brief Bounded copyable cause chain; omitted saturates at SIZE_MAX. */
typedef struct tlv_diagnostic_causes {
    size_t count;   /**< Retained outermost frames, at most TLV_DIAGNOSTIC_CAUSES_MAX. */
    size_t omitted; /**< Number of inner frames lost to capacity limits. */
    tlv_diagnostic_cause_t frames[TLV_DIAGNOSTIC_CAUSES_MAX]; /**< Inline frames. */
} tlv_diagnostic_causes_t;

/** @brief Common output for callbacks whose diagnostic producer is unknown.
 * Existing Reader/Writer diagnostics remain leaves. Higher modules adopt this
 * envelope when their callback boundaries are extracted.
 */
typedef struct tlv_diagnostic_envelope {
    tlv_diagnostic_t diagnostic;    /**< Primary result, location, path and context. */
    tlv_diagnostic_causes_t causes; /**< Delegated detail, outermost first. */
} tlv_diagnostic_envelope_t;

/** @brief Clear a chain; NULL is a no-op. */
OPENTLV_CORE_API void tlv_diagnostic_causes_init(tlv_diagnostic_causes_t* causes);

/** @brief Append copied detail, preserving outermost frames on overflow.
 * causes, domain, domain->name and detail are required. Domain id, type and
 * size must be nonzero, and size must fit TLV_DIAGNOSTIC_DETAIL_MAX. detail
 * must not overlap causes. Invalid input returns NULL_ARG or INVALID_ARG
 * without mutation. A full or already truncated chain increments omitted
 * and returns BUFFER_TOO_SHORT; successful append returns OK.
 */
OPENTLV_CORE_API tlv_result_t tlv_diagnostic_causes_append(tlv_diagnostic_causes_t* causes,
                                                           const tlv_diagnostic_domain_t* domain,
                                                           uint16_t type, const void* detail,
                                                           size_t size);

/** @brief Copy a matching payload into caller-owned typed storage.
 * Required pointers must be non-NULL. Identity matches domain id and name,
 * detail type and exact size. Mismatch or malformed input returns INVALID_ARG
 * and leaves detail unchanged. The destination must not overlap cause.
 * A successful copy returns OK and avoids alignment and aliasing assumptions.
 */
OPENTLV_CORE_API tlv_result_t tlv_diagnostic_cause_copy(const tlv_diagnostic_cause_t* cause,
                                                        const tlv_diagnostic_domain_t* domain,
                                                        uint16_t type, void* detail, size_t size);

#ifdef __cplusplus
}
#endif
#endif
