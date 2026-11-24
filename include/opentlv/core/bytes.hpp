// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_BYTES_HPP
#define OPENTLV_CORE_BYTES_HPP

#include <cstddef>
#include <cstdint>

namespace opentlv::core {

/** Borrowed input buffer. Keep its bytes alive and unchanged while in use. */
struct byte_view {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

/** Borrowed output buffer. Keep its storage alive and writable while in use. */
struct mutable_byte_view {
    std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

} // namespace opentlv::core

#endif /* OPENTLV_CORE_BYTES_HPP */
