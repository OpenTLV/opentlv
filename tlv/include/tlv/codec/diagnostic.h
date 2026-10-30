// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#ifndef OPENTLV_CODEC_DIAGNOSTIC_H
#define OPENTLV_CODEC_DIAGNOSTIC_H
#include "tlv/codec/export.h"
#include "tlv/diagnostic/cause.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @file
 * @brief Optional allocation-free conversion diagnostics and delegated causes.
 */
/** @brief Conversion operation at the diagnostic boundary. */
typedef enum tlv_codec_operation {
    TLV_CODEC_OP_DECODE, /**< Decode complete Value or structure bytes. */
    TLV_CODEC_OP_ENCODE, /**< Encode an application representation. */
    TLV_CODEC_OP_MEASURE /**< Validate and measure without writing. */
} tlv_codec_operation_t;

/**
 * @brief Returns the diagnostic name of a codec operation.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Available whenever OpenTLV::Codec is linked.
 */
OPENTLV_CODEC_API const char* tlv_codec_operation_string(tlv_codec_operation_t value);
/** @brief Legacy cause vocabulary for baseline mapping; new diagnostics use cause frames. */
typedef enum tlv_codec_cause {
    TLV_CODEC_CAUSE_NONE,   /**< No lower-layer detail was supplied. */
    TLV_CODEC_CAUSE_READER, /**< Reader detail is active. */
    TLV_CODEC_CAUSE_SCHEMA  /**< Schema detail is active. */
} tlv_codec_cause_t;

/**
 * @brief Returns the diagnostic name of a codec cause.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Available whenever OpenTLV::Codec is linked.
 */
OPENTLV_CODEC_API const char* tlv_codec_cause_string(tlv_codec_cause_t value);
/** @brief Detected callback contract violation, independent of a returned failure. */
typedef enum tlv_codec_violation {
    TLV_CODEC_VIOLATION_NONE,   /**< No detected callback breach. */
    TLV_CODEC_VIOLATION_RESULT, /**< Unknown or forbidden control result. */
    TLV_CODEC_VIOLATION_SIZE,   /**< Successful output exceeds caller storage. */
    TLV_CODEC_VIOLATION_TYPE,   /**< Successful conversion has the wrong type or span. */
    TLV_CODEC_VIOLATION_UTF8    /**< Successful text result is not valid UTF-8. */
} tlv_codec_violation_t;

/**
 * @brief Returns the diagnostic name of a codec violation.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Available whenever OpenTLV::Codec is linked.
 */
OPENTLV_CODEC_API const char* tlv_codec_violation_string(tlv_codec_violation_t value);
/** @brief Typed reason why a conversion rejected an encoded Value or application object.
 * @note The returned result stays authoritative (usually #TLV_ERR_INVALID_VALUE); the
 * reason explains it. Argument, capacity, delegated and callback failures use NONE. */
typedef enum tlv_codec_reason {
    TLV_CODEC_REASON_NONE,   /**< No conversion-specific reason. */
    TLV_CODEC_REASON_LENGTH, /**< The encoded Value length is invalid for the representation. */
    TLV_CODEC_REASON_DIGIT,  /**< A decimal digit or BCD nibble is invalid or misplaced. */
    TLV_CODEC_REASON_TEXT,   /**< Text violates its character set or encoding, such as UTF-8. */
    TLV_CODEC_REASON_RANGE,  /**< A number or field is outside its representable or permitted range.
                              */
    TLV_CODEC_REASON_CANONICAL, /**< A valid encoding is not in the required canonical or minimal
                                   form. */
    TLV_CODEC_REASON_SYNTAX,    /**< The Value's internal structure is malformed. */
    TLV_CODEC_REASON_OBJECT     /**< The application object does not match the representation. */
} tlv_codec_reason_t;

/**
 * @brief Returns the diagnostic name of a codec reason.
 *
 * @param[in] value Diagnostic enum value.
 * @return A static NUL-terminated string, or `"unknown"` for an unrecognized value.
 * @note Never returns NULL or allocates. Do not free or modify the string.
 * Available whenever OpenTLV::Codec is linked.
 */
OPENTLV_CODEC_API const char* tlv_codec_reason_string(tlv_codec_reason_t value);
/** @brief Conversion detail accompanying one shared common diagnostic.
 * @note All borrows must outlive copies. Delegated evidence belongs to the diagnostic cause chain.
 */
typedef struct tlv_codec_detail {
    tlv_codec_operation_t operation; /**< Conversion operation. */
    int32_t reported; /**< Raw provider result, including unknown integers and OK for a bad
                          success payload. Kept outside the enum domain for safe FFI/C++ reads. */
    tlv_codec_violation_t violation; /**< Detected callback breach. */
    tlv_codec_reason_t reason;       /**< Why the conversion rejected the Value or object. */
    const char* representation;      /**< Optional borrowed static representation name. */
} tlv_codec_detail_t;
/** @brief Copyable conversion diagnostic with one common location/path.
 * @note Caller-owned and optional. NULL avoids collecting detail. Callback diagnostics
 * must not point to temporary stack storage. All non-OK publications carry the returned
 * result in diagnostic.code; unknown detail/location remain unset.
 */
typedef struct tlv_codec_diagnostic {
    tlv_diagnostic_t diagnostic;    /**< Common result, location and borrowed contexts. */
    tlv_codec_detail_t codec;       /**< Codec-owned conversion detail. */
    tlv_diagnostic_causes_t causes; /**< Producer-owned delegated details, outermost first. */
} tlv_codec_diagnostic_t;
/** @brief Clear optional conversion evidence for a new operation.
 * @param[out] diagnostic Optional destination.
 * @param[in] operation Conversion operation.
 */
OPENTLV_CODEC_API void tlv_codec_diagnostic_init(tlv_codec_diagnostic_t* diagnostic,
                                                 tlv_codec_operation_t operation);
/** @brief Publish a conversion result without discarding supplied cause evidence.
 * @param[out] diagnostic Optional initialized destination.
 * @param[in] result Result to publish unchanged.
 * @return result. Decode may preserve NEED_MORE_DATA with informational severity.
 */
static inline tlv_result_t tlv_codec_diagnostic_result(tlv_codec_diagnostic_t* diagnostic,
                                                       tlv_result_t result) {
    if (diagnostic) {
        if (diagnostic->codec.violation == TLV_CODEC_VIOLATION_NONE)
            diagnostic->codec.reported = result;
        diagnostic->diagnostic.code = result;
        diagnostic->diagnostic.severity = result == TLV_OK || result == TLV_NEED_MORE_DATA
                                              ? TLV_DIAGNOSTIC_SEVERITY_INFO
                                              : TLV_DIAGNOSTIC_SEVERITY_ERROR;
    }
    return result;
}
/** @brief Publish a rejected Value or object with its typed reason.
 * @param[out] diagnostic Optional initialized destination.
 * @param[in] result Result to publish unchanged, usually #TLV_ERR_INVALID_VALUE.
 * @param[in] reason Why the conversion rejected its input.
 * @return result.
 */
static inline tlv_result_t tlv_codec_diagnostic_reason(tlv_codec_diagnostic_t* diagnostic,
                                                       tlv_result_t result,
                                                       tlv_codec_reason_t reason) {
    if (diagnostic) diagnostic->codec.reason = reason;
    return tlv_codec_diagnostic_result(diagnostic, result);
}
/** @brief Publish a rejected Value with its typed reason and the offending Value bytes.
 * @param[out] diagnostic Optional initialized destination.
 * @param[in] result Result to publish unchanged, usually #TLV_ERR_INVALID_VALUE.
 * @param[in] reason Why the conversion rejected its input.
 * @param[in] kind #TLV_LOCATION_POINT or #TLV_LOCATION_SPAN.
 * @param[in] begin Inclusive offset from the first Value byte the codec received.
 * @param[in] end Exclusive span end, or `begin` for a point.
 * @return result.
 * @note The location has domain #TLV_LOCATION_VALUE. A caller that knows where
 * the Value starts places it in its own coordinates with tlv_location_resolve().
 * Invalid coordinates leave the location unknown without changing the result.
 */
static inline tlv_result_t tlv_codec_diagnostic_reason_at(tlv_codec_diagnostic_t* diagnostic,
                                                          tlv_result_t result,
                                                          tlv_codec_reason_t reason,
                                                          tlv_location_kind_t kind, size_t begin,
                                                          size_t end) {
    if (diagnostic)
        tlv_location_set(&diagnostic->diagnostic.location, TLV_LOCATION_VALUE, kind, begin, end);
    return tlv_codec_diagnostic_reason(diagnostic, result, reason);
}
/** @brief Publish Codec evidence in a producer-independent callback envelope.
 * NULL output skips evidence; NULL input clears output. Detail and nested cause
 * are copied by value, retaining the producer's existing borrowed lifetimes.
 * Output and input must not overlap. The outermost frames are retained; omitted
 * inner frames are counted with saturation at SIZE_MAX. */
OPENTLV_CODEC_API void tlv_codec_diagnostic_export(tlv_diagnostic_envelope_t* output,
                                                   const tlv_codec_diagnostic_t* input);
/** @brief Copy Codec detail from the outer frame; delegated frames remain in the chain.
 * Returns NULL_ARG for NULL pointers or INVALID_ARG for a foreign outer frame.
 * Output remains unchanged on failure. */
OPENTLV_CODEC_API tlv_result_t tlv_codec_detail_import(const tlv_diagnostic_causes_t* causes,
                                                       tlv_codec_detail_t* output);

/** @brief Append copied Codec detail to a chain using the registered Codec domain.
 * @param[in,out] causes Destination chain; required.
 * @param[in] detail Codec-owned leaf detail; required.
 * @return OK, NULL_ARG, INVALID_ARG or BUFFER_TOO_SHORT as for tlv_diagnostic_causes_append().
 */
OPENTLV_CODEC_API tlv_result_t tlv_codec_detail_attach(tlv_diagnostic_causes_t* causes,
                                                       const tlv_codec_detail_t* detail);
/** @brief Copy a Codec frame into typed caller storage without interpreting foreign payloads.
 * @param[in] cause Source frame; required.
 * @param[out] detail Destination, unchanged on failure; required.
 * @return OK, NULL_ARG or INVALID_ARG as for tlv_diagnostic_cause_copy().
 */
OPENTLV_CODEC_API tlv_result_t tlv_codec_detail_copy(const tlv_diagnostic_cause_t* cause,
                                                     tlv_codec_detail_t* detail);
#ifdef __cplusplus
}
#endif
#endif
