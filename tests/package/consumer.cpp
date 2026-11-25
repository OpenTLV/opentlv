#include <opentlv/io/reader/reader.hpp>
#include <opentlv/io/writer/writer.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#if __cplusplus != 201703L && (!defined(_MSVC_LANG) || _MSVC_LANG != 201703L)
#error "Expected the OpenTLV target to select C++17"
#endif

int main()
{
    namespace core = opentlv::core;
    const std::uint8_t tag[] = {0xa5};
    core::element_view element{{tag, sizeof tag}, {}};
    const core::fixed_format format{{1}, {2, core::byte_order::big_endian}};
    const std::uint8_t payload[] = {0xab};
    const std::uint8_t expected[] = {0xa5, 0x00, 0x01, 0xab};
    std::uint8_t wire[sizeof expected]{};
    opentlv::io::writer writer;
    if (core::make_value_view(payload, sizeof payload, element.value) != core::result::ok)
        return EXIT_FAILURE;
    if (writer.init(format, {wire, sizeof wire}) != core::result::ok ||
        writer.write(element) != core::result::ok || writer.position() != sizeof expected ||
        std::memcmp(wire, expected, sizeof expected) != 0)
        return EXIT_FAILURE;

    opentlv::io::reader reader;
    core::element_view decoded{};
    if (reader.init(format, {wire, writer.position()}) != core::result::ok ||
        reader.next(decoded) != core::result::ok)
        return EXIT_FAILURE;
    if (decoded.tag.data != wire || decoded.value.size != 1 || decoded.value.data[0] != payload[0])
        return EXIT_FAILURE;

    core::diagnostic diagnostic{};
    if (reader.next(decoded, &diagnostic) != core::result::end ||
        diagnostic.result != core::result::end || diagnostic.field != core::field::none ||
        core::result_category(diagnostic.result) != core::category::end ||
        std::strcmp(core::result_string(diagnostic.result), "end of input") != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
