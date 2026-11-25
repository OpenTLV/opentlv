// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLV_HPP
#define OPENTLV_TLV_HPP

/**
 * @file
 * @brief C++17 umbrella header for the Fixed TLV Foundation.
 *
 * Exposes the typed Core, Reader and Writer interfaces and the public C declarations.
 */

#if !defined(__cplusplus) || \
    (__cplusplus < 201703L && (!defined(_MSVC_LANG) || _MSVC_LANG < 201703L))
#error "opentlv/tlv.hpp requires C++17"
#endif

#include "opentlv/tlv.h"
#include "opentlv/core/bytes.hpp"
#include "opentlv/core/formats/fixed.hpp"
#include "opentlv/io/reader/reader.hpp"
#include "opentlv/io/writer/writer.hpp"

#endif /* OPENTLV_TLV_HPP */
