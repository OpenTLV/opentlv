// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLVPP_ERROR_HPP
#define OPENTLV_TLVPP_ERROR_HPP
#include "tlv/result.h"
#include "tlv/diagnostic.h"
#include <cstddef>

/** @file
 * @brief Allocation-free C++ operation status and error context.
 */
namespace tlv {
class tag;
/// @cond INTERNAL
namespace detail {
struct error_access;
}
/// @endcond
/**
 * @brief Canonical operation outcomes, independent of native enum types.
 *
 * `end` and `need_more_data` are control statuses, not failures.
 */
enum class errc {
    ok = TLV_OK,                                 /**< Operation completed. */
    end = TLV_END,                               /**< Normal end of iteration; no element. */
    truncated = TLV_ERR_TRUNCATED,               /**< Final input ends inside an element. */
    buffer_too_short = TLV_ERR_BUFFER_TOO_SHORT, /**< Destination or workspace too small. */
    invalid_length = TLV_ERR_INVALID_LENGTH,     /**< Invalid wire length. */
    null_argument = TLV_ERR_NULL_ARG,            /**< Missing required storage. */
    out_of_memory = TLV_ERR_OUT_OF_MEMORY,       /**< Allocation failed. */
    invalid_tag = TLV_ERR_INVALID_TAG,           /**< Invalid identifier. */
    visitor = TLV_ERR_VISITOR,                   /**< Visitor requested failure. */
    limit = TLV_ERR_LIMIT,                       /**< Configured limit exceeded. */
    schema = TLV_ERR_SCHEMA,                     /**< Schema violation. */
    callback = TLV_ERR_CALLBACK,                 /**< Callback contract violation. */
    invalid_state = TLV_ERR_INVALID_STATE,       /**< Operation forbidden by lifecycle state. */
    invalid_argument = TLV_ERR_INVALID_ARG,      /**< Invalid argument. */
    invalid_tag_size = TLV_ERR_INVALID_TAG_SIZE, /**< Invalid identifier width. */
    syntax = TLV_ERR_SYNTAX,                     /**< Text violates the requested grammar. */
    overflow = TLV_ERR_OVERFLOW,                 /**< Numeric overflow. */
    invalid_value = TLV_ERR_INVALID_VALUE,       /**< Invalid semantic value. */
    unsupported =
        TLV_ERR_UNSUPPORTED, /**< Requested capability or representation is not implemented. */
    invalid_schema = TLV_ERR_INVALID_SCHEMA, /**< Invalid schema definition. */
    native_size = TLV_ERR_NATIVE_SIZE,       /**< Logical size exceeds address space. */
    need_more_data = TLV_NEED_MORE_DATA      /**< Non-final input needs continuation. */
};
/** @brief Name an operation status using immutable program-lifetime text; never allocates. */
inline const char* message(errc code) noexcept {
    return tlv_result_string(static_cast<tlv_result_t>(code));
}
/** @brief Stable symbolic spelling of a canonical status, for logs and serialized diagnostics. */
inline const char* name(errc code) noexcept {
    switch (code) {
        case errc::ok: return "TLV_OK";
        case errc::end: return "TLV_END";
        case errc::truncated: return "TLV_ERR_TRUNCATED";
        case errc::buffer_too_short: return "TLV_ERR_BUFFER_TOO_SHORT";
        case errc::invalid_length: return "TLV_ERR_INVALID_LENGTH";
        case errc::null_argument: return "TLV_ERR_NULL_ARG";
        case errc::out_of_memory: return "TLV_ERR_OUT_OF_MEMORY";
        case errc::invalid_tag: return "TLV_ERR_INVALID_TAG";
        case errc::visitor: return "TLV_ERR_VISITOR";
        case errc::limit: return "TLV_ERR_LIMIT";
        case errc::schema: return "TLV_ERR_SCHEMA";
        case errc::callback: return "TLV_ERR_CALLBACK";
        case errc::invalid_state: return "TLV_ERR_INVALID_STATE";
        case errc::invalid_argument: return "TLV_ERR_INVALID_ARG";
        case errc::invalid_tag_size: return "TLV_ERR_INVALID_TAG_SIZE";
        case errc::syntax: return "TLV_ERR_SYNTAX";
        case errc::overflow: return "TLV_ERR_OVERFLOW";
        case errc::invalid_value: return "TLV_ERR_INVALID_VALUE";
        case errc::unsupported: return "TLV_ERR_UNSUPPORTED";
        case errc::invalid_schema: return "TLV_ERR_INVALID_SCHEMA";
        case errc::native_size: return "TLV_ERR_NATIVE_SIZE";
        case errc::need_more_data: return "TLV_NEED_MORE_DATA";
    }
    return "TLV_ERR_UNKNOWN";
}
/** @brief Subsystem that produced an error. */
enum class operation {
    unspecified, /**< No operation context was supplied. */
    format,      /**< Wire Format operation. */
    reader,      /**< Sequential or tree reading. */
    writer,      /**< Sequential or tree writing. */
    codec,       /**< Value conversion. */
    schema,      /**< Structural validation. */
    document,    /**< Owning tree operation. */
    query        /**< Query compilation or execution. */
};
/** @brief Common diagnostic severity across all C++ operations. */
enum class severity {
    error = TLV_DIAGNOSTIC_SEVERITY_ERROR,     /**< Operation failed. */
    warning = TLV_DIAGNOSTIC_SEVERITY_WARNING, /**< Recoverable observation. */
    info = TLV_DIAGNOSTIC_SEVERITY_INFO        /**< Informational observation. */
};
/** @brief Meaning of a Schema failure's byte offset. */
enum class location_kind {
    unknown = TLV_LOCATION_UNKNOWN,     /**< No byte location. */
    point = TLV_LOCATION_POINT,         /**< Existing element evidence. */
    span = TLV_LOCATION_SPAN,           /**< Half-open evidence range. */
    scope_end = TLV_LOCATION_SCOPE_END, /**< End of enclosing scope. */
    insertion = TLV_LOCATION_INSERTION  /**< Missing ordered content. */
};
/** @brief Coordinate space of diagnostic evidence. */
enum class location_domain {
    unknown = TLV_LOCATION_DOMAIN_UNKNOWN, /**< No known source. */
    input = TLV_LOCATION_INPUT,            /**< Input wire bytes. */
    output = TLV_LOCATION_OUTPUT,          /**< Would-be output wire bytes. */
    expression = TLV_LOCATION_EXPRESSION,  /**< Query expression bytes. */
    definition = TLV_LOCATION_DEFINITION,  /**< Definition text bytes. */
    value = TLV_LOCATION_VALUE             /**< Bytes of one Value, counted from its first byte. */
};
/** @brief Allocation-free primary location value. */
using location = tlv_location_t;
/** @brief Read a location's coordinate domain. */
inline location_domain domain(const location& value) noexcept {
    return static_cast<location_domain>(value.domain);
}
/** @brief Read a location's anchor kind. */
inline location_kind kind(const location& value) noexcept {
    return static_cast<location_kind>(value.kind);
}
/** @brief Static human-readable coordinate-domain name. */
inline const char* message(location_domain value) noexcept {
    return tlv_location_domain_string(static_cast<tlv_location_domain_t>(value));
}
/** @brief Static human-readable location-kind name. */
inline const char* message(location_kind value) noexcept {
    return tlv_location_kind_string(static_cast<tlv_location_kind_t>(value));
}
/** @brief Translate both coordinates; overflow clears only the optional location. */
inline void translate_location(location& value, size_t origin) noexcept {
    tlv_location_translate(&value, origin);
}
/**
 * @brief Place Value-local evidence in the coordinate space that contains the Value.
 * @param value Location; only a known location_domain::value location changes.
 * @param origin Offset of the Value's first byte in `enclosing_domain`.
 * @param enclosing_domain Enclosing coordinate space, such as location_domain::input.
 * @note An invalid domain leaves the location unchanged; overflow clears only the
 *       optional location.
 */
inline void resolve_location(location& value, size_t origin,
                             location_domain enclosing_domain) noexcept {
    tlv_location_resolve(&value, origin, static_cast<tlv_location_domain_t>(enclosing_domain));
}
/**
 * @brief Allocation-free operation error with an optional primary location.
 *
 * location() keeps the coordinate domain and anchor kind of the evidence; read
 * `begin` only when its kind is not location_kind::unknown, and interpret it in
 * the reported domain (input, would-be output, Query expression or Value bytes).
 * Text is borrowed, normally from immutable program-lifetime library strings.
 * Custom descriptions must outlive every error copy. Numeric context is copied.
 * @note Every error contains a full inline diagnostic path. Its storage also
 * contributes to sizeof(expected<T, error>) for successful results, although
 * success does not construct an error. Check sizes when budgeting stack or retained results.
 */
struct error {
    /** @brief Borrowed NUL-terminated description, never null for library errors. */
    const char* message() const noexcept {
        return message_;
    }
    /** @brief Construct an error with a program-lifetime canonical description. */
    explicit error(errc value, operation stage = operation::unspecified) noexcept
        : code_(static_cast<tlv_result_t>(value)), message_(tlv::message(value)), stage_(stage) {}
    /** @brief Construct an interoperability error with borrowed description. */
    error(tlv_result_t value, const char* description) noexcept
        : code_(value), message_(description ? description : tlv_result_string(value)) {}
    /** @brief Translate a native code without allocating. */
    static error from_c(tlv_result_t value) noexcept {
        return error(static_cast<errc>(value));
    }
    /** @brief Canonical C++ status, including distinct EOF and resumable input. */
    errc status() const noexcept {
        return static_cast<errc>(code_);
    }
    /** @brief Subsystem responsible for this failure. */
    operation stage() const noexcept {
        return stage_;
    }
    /** @brief Severity of this diagnostic; operation errors default to error. */
    tlv::severity severity() const noexcept {
        return severity_;
    }
    /** @brief Complete primary evidence location, including its domain and range.
     * @return Unknown kind when no evidence is available; otherwise `begin` and `end`
     *         are coordinates in the location's domain, not necessarily input offsets. */
    // Return an independent snapshot, including when called on a temporary error.
    // cppcheck-suppress returnByReference
    tlv_location_t location() const noexcept {
        return location_;
    }
    /** @brief Whether an enclosing path was tracked, including a known empty path. */
    bool has_path() const noexcept {
        return has_path_;
    }
    /** @brief Whether tag() identifies the failing element; identifier bytes remain borrowed. */
    bool has_tag() const noexcept {
        return has_tag_;
    }
    /** @brief Borrow the failing identifier, or an absent identifier when unavailable. */
    tlv::tag tag() const noexcept;
    /** @brief Number of enclosing identifiers copied into this error. */
    size_t depth() const noexcept {
        return path_.length;
    }
    /** @brief Number of innermost enclosing identifiers omitted after the retained outer prefix. */
    size_t omitted_depth() const noexcept {
        return path_.omitted;
    }
    /** @brief Borrow an enclosing identifier; out-of-range indices return an absent identifier. */
    tlv::tag ancestor(size_t index) const noexcept;
    /** @brief Optional borrowed expected constraint description. */
    const char* expected() const noexcept {
        return expected_;
    }
    /** @brief Optional borrowed observed-value description. */
    const char* actual() const noexcept {
        return actual_;
    }
    /** @brief Return a copy annotated with a subsystem, without inventing a location. */
    error during(operation current_stage) const noexcept {
        auto copy = *this;
        copy.stage_ = current_stage;
        return copy;
    }
    /** @brief Return an enriched copy without modifying the original or allocating. */
    error at(std::size_t offset, operation current_stage) const noexcept {
        error copy = *this;
        tlv_location_set(&copy.location_,
                         current_stage == operation::writer  ? TLV_LOCATION_OUTPUT
                         : current_stage == operation::query ? TLV_LOCATION_EXPRESSION
                                                             : TLV_LOCATION_INPUT,
                         TLV_LOCATION_POINT, offset, offset);
        copy.stage_ = current_stage;
        return copy;
    }

private:
    tlv_result_t          code_;
    const char*           message_;
    operation             stage_ = operation::unspecified;
    tlv_location_t        location_{};
    bool                  has_path_ = false;
    tlv::severity         severity_ = tlv::severity::error;
    tlv_tag_t             tag_{};
    tlv_diagnostic_path_t path_{};
    bool                  has_tag_ = false;
    const char*           expected_ = nullptr;
    const char*           actual_ = nullptr;
    friend struct detail::error_access;
};
/** @brief Project an ordinary error into the common diagnostic contract without allocation. */
inline error to_error(const error& value) noexcept {
    return value;
}
/** @brief Project a specialized failure while retaining its detailed original error object.
 * @tparam Failure Specialized error exposing failure() returning tlv::error.
 * @param value Error from Format, Writer, typed fields, Query or a traversal exception.
 * @return Allocation-free common status, description and available source context.
 */
template <typename Failure> inline error to_error(const Failure& value) noexcept {
    return value.failure();
}
/// @cond INTERNAL
namespace detail {
struct error_access {
    static error diagnostic(const tlv_diagnostic_t& value, operation stage,
                            const tlv_tag_t*             tag = nullptr,
                            const tlv_diagnostic_path_t* path = nullptr) noexcept {
        error result(static_cast<errc>(value.code), stage);
        result.location_ = value.location;
        result.has_path_ = value.has_path != 0;
        if (path)
            result.path_ = *path;
        else if (value.has_path)
            result.path_ = value.path;
        result.severity_ = static_cast<tlv::severity>(value.severity);
        if (tag) {
            result.tag_ = *tag;
            result.has_tag_ = true;
        }
        result.expected_ = value.expected;
        result.actual_ = value.actual;
        return result;
    }
};
} // namespace detail
/// @endcond
} // namespace tlv
#endif
