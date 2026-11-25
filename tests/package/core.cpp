#include <opentlv/core/formats/fixed.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

int main()
{
    namespace core = opentlv::core;
    const std::uint8_t tag[] = {0xa5};
    const core::element_view element{{tag, sizeof tag}, {}};
    const core::fixed_format format{{1}, {2, core::byte_order::big_endian}};
    std::size_t native;
    if (core::element_size(format, element, native) != core::result::ok || native != 3 ||
        core::size_to_native(element.value.size, native) != core::result::ok || native != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
