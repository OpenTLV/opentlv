// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLV_H
#define OPENTLV_TLV_H

/**
 * @file
 * @brief Convenience header for the configured C API.
 * @ingroup core
 */

/* Core primitives and generic formats */
#include "tlv/attributes.h"
#include "tlv/compiler.h"
#include "tlv/defaults.h"
#include "tlv/definition.h"
#include "tlv/element.h"
#include "tlv/size.h"
#include "tlv/value.h"
#include "tlv/copy.h"
#include "tlv/diagnostic.h"
#include "tlv/diagnostic/cause.h"
#include "tlv/callback.h"
#include "tlv/utf8.h"
#include "tlv/endian.h"
#include "tlv/format.h"
#include "tlv/field/encoding.h"
#include "tlv/field/fixed.h"
#include "tlv/field/packed.h"
#include "tlv/field/variable.h"
#include "tlv/field/escaped.h"
#include "tlv/formats/packed.h"
#include "tlv/formats/compose.h"
#include "tlv/formats/fixed.h"
#include "tlv/formats/variable.h"
#include "tlv/formats/escaped.h"

/* Core capabilities */
#include "tlv/reader/reader.h"
#include "tlv/reader/tree.h"
#include "tlv/reader/visitor.h"

#include "tlv/writer/writer.h"
#include "tlv/writer/copy.h"
#include "tlv/writer/tree.h"
#include "tlv/generator.h"

#include "tlv/document/document.h"
#include "tlv/query/document.h"

#include "tlv/query/query.h"

#include "tlv/schema/schema.h"
#include "tlv/schema/constraint.h"
#include "tlv/schema/query.h"
#include "tlv/schema/query_document.h"

#include "tlv/codec/codec.h"
#include "tlv/codec/structure.h"

/* Builtin protocols and formats */
#include "tlv/builtins/asn1/ber.h"

#include "tlv/builtins/asn1/der.h"
#include "tlv/builtins/asn1/der_validation.h"

#include "tlv/builtins/emv/emv.h"

#include "tlv/builtins/bluetooth/ad_types.h"

#include "tlv/builtins/lldp/lldp.h"

#endif /* OPENTLV_TLV_H */
