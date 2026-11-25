// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_READER_READER_HPP
#define OPENTLV_IO_READER_READER_HPP

#include "opentlv/core/bytes.hpp"
#include "opentlv/core/diagnostic.hpp"
#include "opentlv/core/formats/fixed.hpp"
#include "opentlv/io/reader/reader.h"
#include <cstddef>

namespace opentlv::io {

/** Read the first element and its encoded size. Both outputs are preserved on
 * end or error. A supplied diagnostic is refreshed on every call. Store the
 * outputs separately from each other and the input. Keep the input bytes
 * alive and unchanged through the last use of the returned view.
 */
[[nodiscard]] inline core::result read(const core::fixed_format &format, core::byte_view input,
                                       core::element_view &element, std::size_t &consumed,
                                       core::diagnostic *details = nullptr) noexcept
{
    const auto config = core::to_c(format);
    tlv_element_t native_element{};
    tlv_result_t code;
    if (details) {
        tlv_diagnostic_t native_details;
        code = tlv_read_with_diagnostic(input.data, input.size, &config, &native_element,
                                        &consumed, &native_details);
        *details = core::from_c(native_details);
    } else {
        code = tlv_read_with_diagnostic(input.data, input.size, &config, &native_element,
                                        &consumed, nullptr);
    }
    if (code == TLV_OK) {
        element = core::from_c(native_element);
    }
    return core::from_c(code);
}

/** A cursor over borrowed input. Copies have independent positions. Keep the
 * input bytes alive and unchanged while using any cursor or returned view.
 */
class reader {
public:
    /** Borrow the input, copy the format and start at byte zero. Failure
     * preserves the current stream and position.
     */
    [[nodiscard]] core::result init(const core::fixed_format &format, core::byte_view input) noexcept
    {
        const auto config = core::to_c(format);
        return core::from_c(tlv_reader_init(&cursor_, input.data, input.size, &config));
    }

    /** After successful initialization, read the next element. End and errors
     * preserve the element and position. Store the outputs separately from
     * each other and the input. A supplied diagnostic is refreshed on every
     * call and its offset is relative to the beginning of the stream.
     */
    [[nodiscard]] core::result next(core::element_view &element,
                                    core::diagnostic *details = nullptr) noexcept
    {
        tlv_element_t native_element{};
        tlv_result_t code;
        if (details) {
            tlv_diagnostic_t native_details;
            code = tlv_reader_next_with_diagnostic(&cursor_, &native_element, &native_details);
            *details = core::from_c(native_details);
        } else {
            code = tlv_reader_next_with_diagnostic(&cursor_, &native_element, nullptr);
        }
        if (code == TLV_OK) {
            element = core::from_c(native_element);
        }
        return core::from_c(code);
    }

    [[nodiscard]] std::size_t position() const noexcept
    {
        return cursor_.pos;
    }

    /** Return the saved format by value, for example to initialize another stream. */
    [[nodiscard]] core::fixed_format format() const noexcept
    {
        return core::from_c(cursor_.format);
    }

private:
    tlv_reader_t cursor_{};
};

} // namespace opentlv::io

#endif /* OPENTLV_IO_READER_READER_HPP */
