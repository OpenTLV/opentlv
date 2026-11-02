// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLVPP_TLV_HPP
#define OPENTLV_TLVPP_TLV_HPP

/**
 * @file tlv.hpp
 * @brief Main include that aggregates the complete tlv++ API.
 *
 * Usage: `#include <opentlv/tlv.hpp>`
 */

/* Core primitives and generic formats */
#include "opentlv/tlv.h"
#include "opentlv/core/format.hpp"
#include "opentlv/core/diagnostic.hpp"
#include "opentlv/core/version.hpp"
#include "opentlv/core/formats/fixed_format.hpp"
#include "opentlv/core/formats/runtime_fixed.hpp"

/* Core capabilities */
#include "opentlv/io/reader/reader.hpp"
#include "opentlv/io/reader/tree.hpp"

#include "opentlv/io/writer/writer.hpp"
#include "opentlv/io/writer/tree.hpp"
#include "opentlv/io/writer/builder.hpp"
#include "opentlv/generator/generator.hpp"

#include "opentlv/document/document.hpp"
#include "opentlv/document/codec.hpp"
#include "opentlv/query/document.hpp"

#include "opentlv/query/query.hpp"
#include "opentlv/query/program.hpp"
#include "opentlv/query/codec.hpp"
#include "opentlv/query/builder.hpp"

#include "opentlv/schema/schema.hpp"

#include "opentlv/codec/dynamic.hpp"
#include "opentlv/codec/typed.hpp"
#include "opentlv/codec/writer.hpp"
#include "opentlv/codec/registry.hpp"
#include "opentlv/codec/structure.hpp"

/* Builtin protocols and formats */
#include "opentlv/builtin/asn1/ber.hpp"
#include "opentlv/builtin/asn1/query.hpp"
#include "opentlv/builtin/asn1/codec.hpp"

#include "opentlv/builtin/asn1/der.hpp"

#include "opentlv/builtin/asn1/cer.hpp"

#include "opentlv/builtin/emv/format.hpp"
#include "opentlv/builtin/emv/query.hpp"
#include "opentlv/builtin/emv/codec.hpp"
#include "opentlv/builtin/emv/dictionary.hpp"

#include "opentlv/builtin/bluetooth/ltv.hpp"
#include "opentlv/builtin/bluetooth/metadata.hpp"
#include "opentlv/builtin/bluetooth/codec.hpp"

#include "opentlv/builtin/nfc/type2.hpp"

#include "opentlv/builtin/dhcp/dhcpv4.hpp"
#include "opentlv/builtin/dhcp/codec.hpp"
#include "opentlv/builtin/dhcp/container.hpp"

#include "opentlv/builtin/lldp/lldp.hpp"
#include "opentlv/builtin/lldp/codec.hpp"

#endif // OPENTLV_TLVPP_TLV_HPP
