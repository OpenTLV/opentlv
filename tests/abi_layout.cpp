#include <opentlv/tlv.h>

#include <cstddef>
#include <type_traits>

// Use the same per-module signatures that identify the expected binary symbols.
#define OPENTLV_ABI_FUNCTION(Return, Name, Args) \
    static_assert(std::is_same_v<decltype(&Name), Return (*) Args>, #Name);
#include "abi/core.h"
#include "abi/io.h"
#undef OPENTLV_ABI_FUNCTION

static_assert(std::is_same_v<tlv_size_t, uint64_t>);
static_assert(TLV_OK == 0 && TLV_END == 1 && TLV_ERR_TRUNCATED == 3 &&
              TLV_ERR_INVALID_TAG_SIZE == 5 && TLV_ERR_INVALID_LENGTH == 6 &&
              TLV_ERR_NULL_ARG == 10 && TLV_ERR_INVALID_ARG == 11 &&
              TLV_ERR_BUFFER_TOO_SHORT == 14 && TLV_ERR_OVERFLOW == 16 &&
              TLV_ERR_NATIVE_SIZE == 17);
static_assert(TLV_CATEGORY_SUCCESS == 0 && TLV_CATEGORY_END == 1 &&
              TLV_CATEGORY_INVALID_ARGUMENT == 2 && TLV_CATEGORY_MALFORMED_DATA == 3 &&
              TLV_CATEGORY_TRUNCATED == 4 && TLV_CATEGORY_CAPACITY == 5 &&
              TLV_CATEGORY_RANGE == 6 && TLV_CATEGORY_UNKNOWN == 7);
static_assert(TLV_FIELD_NONE == 0 && TLV_FIELD_TAG == 1 && TLV_FIELD_LENGTH == 2 &&
              TLV_FIELD_VALUE == 3 && TLV_FIELD_ELEMENT == 4);
static_assert(TLV_BYTE_ORDER_UNKNOWN == 0 && TLV_BYTE_ORDER_BIG_ENDIAN == 1 &&
              TLV_BYTE_ORDER_LITTLE_ENDIAN == 2);

// CI ABI profiles: x86-64 with eight-byte pointers, and Win32 with MSVC packing.
#if defined(_M_X64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_IX86)
namespace {
#ifdef _M_IX86
constexpr std::size_t word = 4;
#else
constexpr std::size_t word = 8;
#endif

template <typename T>
constexpr bool layout(std::size_t size, std::size_t alignment)
{
    return sizeof(T) == size && alignof(T) == alignment;
}

static_assert(layout<void *>(word, word) && layout<std::size_t>(word, word));
static_assert(layout<tlv_size_t>(8, 8) && layout<uintmax_t>(8, 8));
static_assert(layout<tlv_result_t>(4, 4) && layout<tlv_result_category_t>(4, 4));
static_assert(layout<tlv_field_t>(4, 4) && layout<tlv_byte_order_t>(4, 4));

static_assert(layout<tlv_tag_t>(2 * word, word));
static_assert(offsetof(tlv_tag_t, data) == 0 && offsetof(tlv_tag_t, size) == word);
static_assert(layout<tlv_length_t>(2 * word, word));
static_assert(offsetof(tlv_length_t, data) == 0 && offsetof(tlv_length_t, size) == word);
static_assert(layout<tlv_value_t>(16, 8));
static_assert(offsetof(tlv_value_t, data) == 0 && offsetof(tlv_value_t, size) == 8);
static_assert(layout<tlv_element_t>(2 * word + 16, 8));
static_assert(offsetof(tlv_element_t, tag) == 0 && offsetof(tlv_element_t, value) == 2 * word);

static_assert(layout<tlv_fixed_identifier_t>(word, word));
static_assert(offsetof(tlv_fixed_identifier_t, size) == 0);
static_assert(layout<tlv_fixed_length_t>(2 * word, word));
static_assert(offsetof(tlv_fixed_length_t, size) == 0 && offsetof(tlv_fixed_length_t, byte_order) == word);
static_assert(layout<tlv_fixed_format_t>(3 * word, word));
static_assert(offsetof(tlv_fixed_format_t, identifier) == 0 && offsetof(tlv_fixed_format_t, length) == word);

static_assert(layout<tlv_diagnostic_t>(32, 8));
static_assert(offsetof(tlv_diagnostic_t, result) == 0 && offsetof(tlv_diagnostic_t, field) == 4);
static_assert(offsetof(tlv_diagnostic_t, offset) == 8 && offsetof(tlv_diagnostic_t, required) == 16);
static_assert(offsetof(tlv_diagnostic_t, available) == 24);

static_assert(layout<tlv_reader_t>(6 * word, word));
static_assert(offsetof(tlv_reader_t, data) == 0 && offsetof(tlv_reader_t, size) == word);
static_assert(offsetof(tlv_reader_t, pos) == 2 * word && offsetof(tlv_reader_t, format) == 3 * word);
static_assert(layout<tlv_writer_t>(6 * word, word));
static_assert(offsetof(tlv_writer_t, data) == 0 && offsetof(tlv_writer_t, capacity) == word);
static_assert(offsetof(tlv_writer_t, pos) == 2 * word && offsetof(tlv_writer_t, format) == 3 * word);
} // namespace
#endif
