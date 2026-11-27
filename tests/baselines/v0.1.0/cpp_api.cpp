#include <opentlv/tlv.hpp>

// Compile the released inline overloads and member signatures against the
// consumer's headers. C++17 includes noexcept in the function pointer type.
namespace {
using namespace opentlv::core;
using opentlv::io::reader;
using opentlv::io::writer;

template<typename Signature> constexpr bool accepts(Signature)
{
    return true;
}

template<typename Cpp, typename C> constexpr bool value_conversions()
{
    return accepts<C (*)(Cpp) noexcept>(to_c) && accepts<Cpp (*)(C) noexcept>(from_c);
}

template<typename Cpp, typename C> constexpr bool reference_conversions()
{
    return accepts<C (*)(const Cpp &) noexcept>(to_c) &&
           accepts<Cpp (*)(const C &) noexcept>(from_c);
}

static_assert(value_conversions<result, tlv_result_t>());
static_assert(value_conversions<category, tlv_result_category_t>());
static_assert(value_conversions<field, tlv_field_t>());
static_assert(value_conversions<byte_order, tlv_byte_order_t>());
static_assert(value_conversions<tag_view, tlv_tag_t>());
static_assert(value_conversions<length_view, tlv_length_t>());
static_assert(value_conversions<value_view, tlv_value_t>());
static_assert(value_conversions<fixed_identifier, tlv_fixed_identifier_t>());
static_assert(value_conversions<fixed_length, tlv_fixed_length_t>());
static_assert(reference_conversions<element_view, tlv_element_t>());
static_assert(reference_conversions<fixed_format, tlv_fixed_format_t>());
static_assert(reference_conversions<diagnostic, tlv_diagnostic_t>());

static_assert(accepts<category (*)(result) noexcept>(result_category));
static_assert(accepts<const char *(*)(result) noexcept>(result_string));
static_assert(accepts<result (*)(logical_size, std::size_t &) noexcept>(size_to_native));
static_assert(accepts<result (*)(logical_size, logical_size, logical_size &) noexcept>(size_add));
static_assert(accepts<result (*)(const std::uint8_t *, logical_size, value_view &) noexcept>(
    make_value_view));
static_assert(accepts<result (*)(const value_view &) noexcept>(validate));
static_assert(accepts<result (*)(const fixed_length &, length_view, logical_size &,
                                 std::size_t &) noexcept>(read_length));
static_assert(accepts<result (*)(const fixed_length &, logical_size, mutable_byte_view,
                                 std::size_t &) noexcept>(write_length));
static_assert(accepts<result (*)(const fixed_format &, std::size_t &) noexcept>(header_size));
static_assert(accepts<result (*)(const fixed_format &, const element_view &,
                                 std::size_t &) noexcept>(element_size));

static_assert(accepts<result (*)(const fixed_format &, byte_view, element_view &, std::size_t &,
                                 diagnostic *) noexcept>(opentlv::io::read));
static_assert(accepts<result (*)(const fixed_format &, mutable_byte_view, const element_view &,
                                 std::size_t &, diagnostic *) noexcept>(opentlv::io::write));
static_assert(accepts<result (reader::*)(const fixed_format &, byte_view) noexcept>(&reader::init));
static_assert(accepts<result (reader::*)(element_view &, diagnostic *) noexcept>(&reader::next));
static_assert(accepts<std::size_t (reader::*)() const noexcept>(&reader::position));
static_assert(accepts<fixed_format (reader::*)() const noexcept>(&reader::format));
static_assert(
    accepts<result (writer::*)(const fixed_format &, mutable_byte_view) noexcept>(&writer::init));
static_assert(
    accepts<result (writer::*)(const element_view &, diagnostic *) noexcept>(&writer::write));
static_assert(accepts<std::size_t (writer::*)() const noexcept>(&writer::position));
static_assert(accepts<fixed_format (writer::*)() const noexcept>(&writer::format));
} // namespace
