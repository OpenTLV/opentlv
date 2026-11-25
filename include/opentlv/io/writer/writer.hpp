// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_IO_WRITER_WRITER_HPP
#define OPENTLV_IO_WRITER_WRITER_HPP

#include "opentlv/core/bytes.hpp"
#include "opentlv/core/diagnostic.hpp"
#include "opentlv/core/formats/fixed.hpp"
#include "opentlv/io/writer/writer.h"

namespace opentlv::io {

/** Encode one element and set written to its encoded size. Short output
 * sets written to the required size. Other errors preserve written.
 * Every error preserves the destination bytes.
 * Keep source bytes disjoint from the destination range and store the
 * count and optional diagnostic separately from all buffers and inputs.
 */
[[nodiscard]] inline core::result write(const core::fixed_format &format, core::mutable_byte_view output,
                                        const core::element_view &element, std::size_t &written,
                                        core::diagnostic *details = nullptr) noexcept
{
    const auto config = core::to_c(format);
    const auto source = core::to_c(element);
    if (!details) {
        return core::from_c(tlv_write_with_diagnostic(output.data, output.size, &config, &source,
                                                      &written, nullptr));
    }
    tlv_diagnostic_t diagnostic;
    const auto code = tlv_write_with_diagnostic(output.data, output.size, &config, &source,
                                                &written, &diagnostic);
    *details = core::from_c(diagnostic);
    return core::from_c(code);
}

/** A non-copyable cursor borrowing a writable buffer. */
class writer {
public:
    writer() noexcept = default;
    writer(const writer &) = delete;
    writer &operator=(const writer &) = delete;

    /** Copy the format and borrow output. Failure preserves the current stream. */
    [[nodiscard]] core::result init(const core::fixed_format &format, core::mutable_byte_view output) noexcept
    {
        const auto config = core::to_c(format);
        return core::from_c(tlv_writer_init(&cursor_, output.data, output.size, &config));
    }

    /** Append one element. Before the first successful init, return invalid_arg.
     * Keep the source disjoint from the range
     * being written and the diagnostic outside the cursor, element and buffers.
     * Failure preserves the position and bytes.
     */
    [[nodiscard]] core::result write(const core::element_view &element,
                                     core::diagnostic *details = nullptr) noexcept
    {
        // A successfully initialized Fixed TLV format has a nonzero tag width.
        if (cursor_.format.identifier.size == 0) {
            if (details) {
                *details = {core::result::invalid_arg};
            }
            return core::result::invalid_arg;
        }
        const auto source = core::to_c(element);
        if (!details) {
            return core::from_c(tlv_writer_write_with_diagnostic(&cursor_, &source, nullptr));
        }
        tlv_diagnostic_t diagnostic;
        const auto code = tlv_writer_write_with_diagnostic(&cursor_, &source, &diagnostic);
        *details = core::from_c(diagnostic);
        return core::from_c(code);
    }

    [[nodiscard]] std::size_t position() const noexcept { return cursor_.pos; }
    [[nodiscard]] core::fixed_format format() const noexcept { return core::from_c(cursor_.format); }

private:
    tlv_writer_t cursor_{};
};

} // namespace opentlv::io

#endif /* OPENTLV_IO_WRITER_WRITER_HPP */
