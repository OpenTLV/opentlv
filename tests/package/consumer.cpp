#include <opentlv/tlv.hpp>

#include <cstdlib>
#include <cstring>

int main()
{
    namespace core = opentlv::core;
    const uint8_t tag[] = {0xa5};
    core::element_view element{{tag, sizeof tag}, {}};
    const core::fixed_format format{{1}, {2, core::byte_order::big_endian}};
    size_t native;
    if (core::element_size(format, element, native) != core::result::ok || native != 3 ||
        core::size_to_native(element.value.size, native) != core::result::ok || native != 0)
        return EXIT_FAILURE;

    const uint8_t payload[] = {0xab};
    const uint8_t expected[] = {0xa5, 0x00, 0x01, 0xab};
    uint8_t wire[sizeof expected]{};
    tlv_writer_t writer{};
    if (core::make_value_view(payload, sizeof payload, element.value) != core::result::ok)
        return EXIT_FAILURE;
    const auto c_format = core::to_c(format);
    auto c_element = core::to_c(element);
    if (tlv_writer_init(&writer, wire, sizeof wire, &c_format) != TLV_OK ||
        tlv_writer_write(&writer, &c_element) != TLV_OK ||
        writer.pos != sizeof expected || std::memcmp(wire, expected, sizeof expected) != 0)
        return EXIT_FAILURE;

    opentlv::io::reader reader;
    core::element_view decoded{};
    if (reader.init(format, {wire, writer.pos}) != core::result::ok ||
        reader.next(decoded) != core::result::ok)
        return EXIT_FAILURE;
    if (decoded.tag.data != wire || decoded.value.size != 1 || decoded.value.data[0] != payload[0])
        return EXIT_FAILURE;

    core::diagnostic diagnostic{};
    if (reader.next(decoded, &diagnostic) != core::result::end ||
        diagnostic.result != core::result::end || diagnostic.field != core::field::none ||
        tlv_result_category(core::to_c(diagnostic.result)) != TLV_CATEGORY_END ||
        std::strcmp(tlv_result_string(core::to_c(diagnostic.result)), "end of input") != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
