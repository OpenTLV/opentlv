// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLVPP_TLV_HPP
#define OPENTLV_TLVPP_TLV_HPP

/**
 * @file tlv.hpp
 * @brief Main include that aggregates the complete tlv++ API.
 *
 * Usage: `#include <tlv++/tlv.hpp>`
 */

/* Core primitives and generic formats */
#include "tlv/tlv.h"
#include "tlv++/format.hpp"
#include "tlv++/diagnostic.hpp"
#include "tlv++/version.hpp"
#include "tlv++/formats/fixed_format.hpp"
#include "tlv++/formats/runtime_fixed.hpp"

/* Core capabilities */
#include "tlv++/reader/reader.hpp"
#include "tlv++/reader/tree.hpp"

#include "tlv++/writer/writer.hpp"
#include "tlv++/writer/tree.hpp"
#include "tlv++/writer/builder.hpp"
#include "tlv++/generator.hpp"

#include "tlv++/document/document.hpp"
#include "tlv++/document/codec.hpp"
#include "tlv++/query/document.hpp"

#include "tlv++/query/query.hpp"
#include "tlv++/query/program.hpp"
#include "tlv++/query/codec.hpp"
#include "tlv++/query/builder.hpp"

#include "tlv++/schema/schema.hpp"

#include "tlv++/codec/dynamic.hpp"
#include "tlv++/codec/typed.hpp"
#include "tlv++/codec/writer.hpp"
#include "tlv++/codec/registry.hpp"
#include "tlv++/codec/structure.hpp"

/* Builtin protocols and formats */
#include "tlv++/builtins/asn1/ber.hpp"
#include "tlv++/builtins/asn1/query.hpp"
#include "tlv++/builtins/asn1/codec.hpp"

#include "tlv++/builtins/asn1/der.hpp"

#include "tlv++/builtins/asn1/cer.hpp"

#include "tlv++/builtins/emv/format.hpp"
#include "tlv++/builtins/emv/query.hpp"
#include "tlv++/builtins/emv/codec.hpp"
#include "tlv++/builtins/emv/dictionary.hpp"

#include "tlv++/builtins/bluetooth/ltv.hpp"
#include "tlv++/builtins/bluetooth/metadata.hpp"
#include "tlv++/builtins/bluetooth/codec.hpp"

#include "tlv++/builtins/nfc/type2.hpp"

#include "tlv++/builtins/dhcp/dhcpv4.hpp"
#include "tlv++/builtins/dhcp/codec.hpp"
#include "tlv++/builtins/dhcp/container.hpp"

#include "tlv++/builtins/lldp/lldp.hpp"
#include "tlv++/builtins/lldp/codec.hpp"

#endif // OPENTLV_TLVPP_TLV_HPP
