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
#include "opentlv/core/attributes.h"
#include "opentlv/core/compiler.h"
#include "opentlv/io/defaults.h"
#include "opentlv/core/definition.h"
#include "opentlv/core/element.h"
#include "opentlv/core/size.h"
#include "opentlv/core/value.h"
#include "opentlv/core/copy.h"
#include "opentlv/core/diagnostic.h"
#include "opentlv/core/diagnostic/cause.h"
#include "opentlv/core/callback.h"
#include "opentlv/core/utf8.h"
#include "opentlv/core/endian.h"
#include "opentlv/core/format.h"
#include "opentlv/core/field/encoding.h"
#include "opentlv/core/field/fixed.h"
#include "opentlv/core/field/packed.h"
#include "opentlv/core/field/variable.h"
#include "opentlv/core/field/escaped.h"
#include "opentlv/core/formats/packed.h"
#include "opentlv/core/formats/compose.h"
#include "opentlv/core/formats/fixed.h"
#include "opentlv/core/formats/variable.h"
#include "opentlv/core/formats/escaped.h"

/* Core capabilities */
#include "opentlv/io/reader/reader.h"
#include "opentlv/io/reader/tree.h"
#include "opentlv/io/reader/visitor.h"

#include "opentlv/io/writer/writer.h"
#include "opentlv/io/writer/copy.h"
#include "opentlv/io/writer/tree.h"
#include "opentlv/generator/generator.h"

#include "opentlv/document/document.h"
#include "opentlv/query/document.h"

#include "opentlv/query/query.h"

#include "opentlv/schema/schema.h"
#include "opentlv/schema/constraint.h"
#include "opentlv/schema/query.h"
#include "opentlv/schema/query_document.h"

#include "opentlv/codec/codec.h"
#include "opentlv/codec/structure.h"

/* Builtin protocols and formats */
#include "opentlv/builtin/asn1/ber.h"

#include "opentlv/builtin/asn1/der.h"
#include "opentlv/builtin/asn1/der_validation.h"

#include "opentlv/builtin/emv/emv.h"

#include "opentlv/builtin/bluetooth/ad_types.h"

#include "opentlv/builtin/lldp/lldp.h"

#endif /* OPENTLV_TLV_H */
