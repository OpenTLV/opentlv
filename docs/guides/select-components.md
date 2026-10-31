# Build only the components you need

OpenTLV builds only the components you ask for. This guide gives ready-made CMake
recipes for the common selections. For the principle and the option reference, see
[include only what you need](../concepts/architecture.md#include-only-what-you-need) and
[build configuration](../concepts/architecture.md#build-configuration).

Core, IO, Document, Generator, Query, Schema and Codec are independent C libraries. Core owns Definition, Format and Field
Encoding, Element and Layout, byte utilities and generic diagnostics. IO owns
Reader, Writer, traversal and serialization and depends publicly on Core.
Document owns mutable trees and depends publicly on Core and IO.
Generator owns deterministic wire generation, with public Core and private IO
dependencies. IO does not build or link Generator.

## Core, IO and Document

Select functionality by linking the target you use:

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Core IO)
target_link_libraries(my_parser PRIVATE OpenTLV::IO)
# For generic primitives without Reader/Writer:
target_link_libraries(my_format_tool PRIVATE OpenTLV::Core)
```

The same targets are available through `add_subdirectory()` and FetchContent.
A Core-only package consumer can request just `COMPONENTS Core`; no IO or C++
library is loaded. All extracted modules support static and shared builds, selected by
`OPENTLV_BUILD_SHARED_LIBS`. A library-only build does not require a C++ compiler,
even when the compatibility C++ facade is enabled.

Build only the required target from a checkout:

```sh
cmake -S . -B build/modules -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build/modules --target opentlv_core
# Builds Core and IO, without the remaining library implementations:
cmake --build build/modules --target opentlv_io
# Builds Document and its Core/IO dependencies only:
cmake --build build/modules --target opentlv_document
# Builds Generator and its Core/IO dependencies only:
cmake --build build/modules --target opentlv_generator
```

`OPENTLV_READER`, `OPENTLV_WRITER`, `OPENTLV_DOCUMENT`, `OPENTLV_QUERY`, `OPENTLV_SCHEMA` and `OPENTLV_CODEC` are no longer CMake
options. Supplying any of them
is an error; remove them from presets and existing caches (or use a fresh build
directory). IO always includes both APIs. For transition compatibility,
`tlv/config.h` retains the constant macros with value 1 and their runtime mirrors;
they do not select sources or indicate which modules an application linked.

Existing CMake consumers of `OpenTLV::tlv` and `OpenTLV::tlv++` receive Core, IO,
Document, Generator, Query, Codec, Schema and their extracted integrations transitively. Direct library consumers must link the dependencies too (static
link order: `tlv`, `opentlv_schema_query_document`, `opentlv_schema_query`,
`opentlv_schema_number`, `opentlv_codec_structure`, `opentlv_query_document`, `opentlv_query_codec`, `opentlv_query`, `opentlv_document`, `opentlv_generator`, `opentlv_codec`, `opentlv_schema`, `opentlv_io`, `opentlv_core`). Shared deployments must distribute
all linked libraries. Rebuild consumers after the split; the old single-library
binary distribution is not preserved.

If every builtin is disabled, the transitional `tlv` target contains no
implementation symbols and is an empty compatibility archive, including in a
shared build. Its extracted dependencies remain shared; no empty DLL is required.

Public Reader/Writer headers and function signatures remain at their existing
paths. `tlv_copy_element()` now belongs to `tlv/writer/copy.h`; Core `tlv/copy.h`
contains raw byte/value copies only. `tlv/tlv.h` includes both. New public utility
headers are `tlv/utf8.h`, `tlv/callback.h` and `tlv/diagnostic/cause.h`. Module
exports use `OPENTLV_CORE_API`, `OPENTLV_IO_API`, `OPENTLV_DOCUMENT_API` and
`OPENTLV_GENERATOR_API`; CMake propagates the matching
static-build definitions automatically.

## Independent Document

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Document)
target_link_libraries(my_editor PRIVATE OpenTLV::Document)
```

This loads only Document, IO and Core. Include `tlv/document/document.h` for
ownership, topology, mutation, parsing and serialization. Document requires no
Query, Schema, Codec, Generator or builtin implementation; supply a Format.

Include `tlv/query/document.h` explicitly for Document Query operations previously
declared by `tlv/document/document.h`. The aggregate `tlv/tlv.h` still includes
both. Link `OpenTLV::QueryDocument` for these operations; linking Document alone
does not provide Query symbols.
The existing C++ facade remains part of the compatibility target.

`tlv/document/integration.h` defines the public integration boundary: effective
options and allocator access, live-node ownership checks, callback activity and
nested callback lifetime guards. Integrations use existing node traversal,
identity, retirement epoch, revision and mutation APIs. Guarded fallible edits
fail with `INVALID_STATE`; erasure and destruction are deferred until the
outermost guard exits. No Document private header is needed by Query.

## Independent Codec

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Codec)
target_link_libraries(my_values PRIVATE OpenTLV::Codec)
```

This loads only Codec and Core. A target-only source build with
`cmake --build build/modules --target opentlv_codec` compiles no Reader, Schema,
Query, Document, Generator or builtin implementation. All generic numeric,
digit, text, IPv4 and byte conversions, callbacks and measurement remain available.
Public headers retain their `tlv/codec/` paths and use `OPENTLV_CODEC_API`.

Link `OpenTLV::QueryCodec` and include `tlv/query/adapters.h` for generic Query
conversion providers. This integration depends explicitly on Core, IO, Query
and Codec. Neither base module links the integration.

`tlv/codec/structure.h` belongs to the Codec-owned `codec-structure` integration,
which composes Codec, Schema and IO. Link `OpenTLV::CodecStructure`; the aggregate
continues to provide structured validation and conversion without changing
wire behavior. No base Codec dependency points back to Schema or the aggregate.

Codec diagnostics now store a leaf `codec` detail and a separate `causes` chain.
Use the producer's `tlv_reader_detail_copy()` or `tlv_schema_detail_copy()` on a
cause frame instead of `codec.cause` and `codec.detail`. Common locations, paths,
expected/actual text and conversion operation, raw result, reason, representation
and callback violation are preserved. `tlv_codec_diagnostic_export()` prepends
the Codec frame while preserving delegated frames and bounded omission counts;
`tlv_codec_detail_import()` copies only the outer Codec leaf. Legacy cause enum
names remain available for the approved baseline mapping. Rebuild binary consumers.
C++ `codec_failure::causes()` exposes the retained chain.

Include `tlv++/codec/writer.hpp` for typed `writer_base::write()` overloads.
`tlv++/codec/typed.hpp` no longer includes Writer. The aggregate `tlv++/tlv.hpp`
includes both. Common C++ error support no longer includes Schema headers.

## Independent Schema

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Schema)
target_link_libraries(my_validator PRIVATE OpenTLV::Schema)
```

Only Schema, IO and Core are loaded. A source build with
`cmake --build build/modules --target opentlv_schema` compiles no Codec, Query,
Document, Generator or builtin implementation. Length constraints, structural
rules, occurrence/order checks, preparation and diagnostic reports remain available.
Supply a Format; builtin Formats are optional consumers of the generic contract.

Select integrations explicitly:

| Target | Header | Behavior |
| --- | --- | --- |
| `OpenTLV::SchemaNumber` | `tlv/schema/number.h` | Schema-backed numeric conversion and length diagnostic composition |
| `OpenTLV::SchemaQuery` | `tlv/schema/query.h` | Contextual assertions over buffers |
| `OpenTLV::SchemaQueryDocument` | `tlv/schema/query_document.h` | Contextual assertions over Document, including D plans |
| `OpenTLV::CodecStructure` | `tlv/codec/structure.h` | Object conversion with structural validation |

Base modules do not depend on these integrations. Rules retain their order,
context selection and execution semantics; assertion failures retain the rule
index, name, tag, path and available source location. Query execution failures
keep their original cause and result. The aggregate includes all integrations.

`tlv_schema_diagnostic_t` keeps Schema findings in `detail.schema`. For
`TLV_SCHEMA_CAUSE_READER` or `TLV_SCHEMA_CAUSE_CODEC`, inspect `detail.causes`
using the producer's `tlv_reader_detail_copy()` or `tlv_codec_detail_copy()`.
Direct `detail.reader` and `detail.codec` members are removed. Diagnostic copies
retain their external borrows; codes, classifications and evidence are unchanged.
The cause discriminator keeps its existing numeric vocabulary for compatibility.

C++ Schema validation, reports and preparation return `schema_failure`, declared
in `tlv++/schema/error.hpp`. It retains `schema_kind()`, `definition()` and
`definition_kind()`, exposes the complete diagnostic with `native()`, and provides
`failure()` / `to_error()` for common status, location and path. Those
Schema-specific accessors no longer belong to `tlv::error`.

## Independent Generator

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Generator)
target_link_libraries(my_generator PRIVATE OpenTLV::Generator)
```

This loads only Generator, IO and Core. Include `tlv/generator.h`; Generator has
no Document, Query, Schema, Codec or builtin library dependency. Supply a readable,
writable Format and caller-owned output and workspace. The module always provides
generation; no Writer capability option selects it.

The existing header-only `tlv++/generator.hpp` belongs to Generator. A C++11
consumer can use it with `OpenTLV::Generator` and the installed C++ headers;
`OPENTLV_BUILD_CXX=ON` installs them. No compiled C++ library is introduced.
The shared C++ support headers remain in the compatibility facade until their
own extraction. Its owned generator still allocates scratch/results, while
native generation and the caller-buffer facade remain allocation-free.

Algorithm version 2, seeds, case indices, candidate order, limits, workspace,
Writer reconstruction checks and errors are unchanged. See the
[generation contract](../reference/c-api.md#deterministic-wire-generation).

## Independent Query

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Query)
target_link_libraries(my_filter PRIVATE OpenTLV::Query)
# For Document evaluation and editing:
find_package(OpenTLV REQUIRED CONFIG COMPONENTS QueryDocument)
target_link_libraries(my_editor PRIVATE OpenTLV::QueryDocument)
```

`Query` loads only Core and IO. It provides exact-path matching, text compilation,
static plans, streaming and retained execution. Build `opentlv_query` to compile
only that dependency closure. `QueryDocument` adds Document through public
traversal, mutation and callback interfaces; Document never links Query.

Include `tlv/query/definition.h` for name resolution, `tlv/query/program.h` for
programs and execution, and `tlv/query/features.h` for the frontend/set-operation
macros and runtime mirrors. `OPENTLV_QUERY_FRONTEND` and
`OPENTLV_QUERY_SET_OPERATIONS` remain Query-internal CMake options.

Codec adapters live in the Query-owned `query-codec` implementation, currently
assembled into the transitional `OpenTLV::tlv` target until Codec extraction.
Include `tlv/query/adapters.h` for them. The base Query target does not compile or
link that implementation. C++ environments start without conversion providers;
include `tlv++/query/codec.hpp` and call `tlv::configure_query_codecs(environment)`
before compilation to add NUM, BCD and TEXT. Existing builtin configuration
functions still add their own providers explicitly.

Query conversion callbacks now receive `tlv_diagnostic_envelope_t*`.
Codec-backed callbacks can publish their original diagnostic using
`tlv_codec_diagnostic_export()`. Query preserves the primary location, expression,
context and path, stores the raw callback result and violation in `reported` and
`violation`, and carries delegated frames in `detail.causes` when `cause` is CODEC
or SCHEMA. Read frames through their producer's copy/import helpers. Direct Reader
failures retain `detail.reader`. This changes the public Query diagnostic ABI and
callback signature; rebuild native consumers and update binding layouts.

## Module and feature checks

Module selection is expressed by target links. The old capability-selection
options are rejected. `python scripts/check_capabilities.py` runs all module
probes with full Query features and with Query frontend/set operations disabled.
`scripts/check_module_isolation.py` builds each module in a fresh tree, checks
symbol/header ownership, and runs relocated installed-package consumers. CI
checks static and shared configurations.

Standalone bindings and tools own their Core dependency checks and pins. Their
build scripts must migrate removed CMake options when adopting this revision.

## Generic core formats

| Format | Availability |
| --- | --- |
| [Configurable fixed-width TLV](../formats/fixed/configurable.md) | Always built; shared C implementation and C++ wrapper |

## Built-in standards

Each protocol family has one independently linkable public target. All family
implementations are available in the package; consumers select their dependencies
by linking targets. Generic modules never link a builtin.

| Target | Component | Dependencies |
| --- | --- | --- |
| `OpenTLV::ASN1` | BER, DER, CER, shared identifiers, strict validation, codecs, DER Schema and Query hooks | Core, IO, Codec, Schema, Query |
| `OpenTLV::EMV` | Framing, DOL, dictionary, codecs, Schema, presentation and Query integration | Core, Codec, Schema, Query; ASN1 and SchemaNumber privately |
| `OpenTLV::Bluetooth` | LTV framing, containers, definitions, Schema and codecs | Core, IO, Codec, Schema |
| `OpenTLV::NFC` | NFC Type 2 framing | Core |
| `OpenTLV::DHCP` | DHCPv4 framing, options, codecs and container validation | Core, Codec, Schema; IO privately |
| `OpenTLV::LLDP` | Framing, definitions, LLDPDU validation and codecs | Core, Codec, Schema; IO privately |

EMV retains its existing ASN.1 identifier helper for DOL. BER, DER and CER form
one ASN1 library; there are no separate BER/DER/CER targets. DHCP contains only
the existing DHCPv4 implementation; no DHCPv6 functionality is added.

Remove `OPENTLV_FORMAT_ASN1`, `OPENTLV_FORMAT_BER`, `OPENTLV_FORMAT_DER`,
`OPENTLV_FORMAT_CER`, `OPENTLV_EMV`, `OPENTLV_BLUETOOTH`, `OPENTLV_NFC`,
`OPENTLV_DHCP` and `OPENTLV_LLDP` from CMake invocations and old caches.
Supplying either ON or OFF now fails with migration guidance. The corresponding
`tlv/config.h` macros and runtime mirrors remain constant 1 for compatibility;
they do not indicate which libraries an application links.

## Recipes

For a generic custom Format and Reader, request only IO (which brings Core):

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS IO)
target_link_libraries(app PRIVATE OpenTLV::IO)
```

For BER, DER or CER, request ASN1:

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS ASN1)
target_link_libraries(app PRIVATE OpenTLV::ASN1)
```

For Bluetooth or EMV, select the relevant family:

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Bluetooth EMV)
target_link_libraries(app PRIVATE OpenTLV::Bluetooth OpenTLV::EMV)
```

NFC framing itself needs only Core. Add IO explicitly when using Reader/Writer:

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS NFC IO)
target_link_libraries(app PRIVATE OpenTLV::NFC OpenTLV::IO)
```

The same target names work with `add_subdirectory` and FetchContent. To build
only NFC and its dependency from source:

```sh
cmake -S . -B build-nfc -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build-nfc --target opentlv_nfc
```

The compatibility targets `OpenTLV::tlv` and `OpenTLV::tlv++` bring all families
transitively. The empty `tlv` compatibility archive remains static even in a
shared-module build, because an export-free Windows DLL cannot be linked.
Direct archive/shared-library consumers must rebuild, link and distribute the
required `opentlv_*` libraries; builtin symbols are no longer defined by `tlv`.

## Footprint and bindings

Static linking selects referenced archive members. A BER-only executable need
not retain DER, CER, codecs or Query hooks. Shared linking loads the complete
ASN1 library and its dependencies. See the measured
[ASN.1 footprint](../development/modules/README.md#asn1-granularity).

The C++ facade remains header-only. Individual builtin C++ headers may require
additional generic integration targets; the compatibility C++ target supplies
them. Standalone language bindings own their pins, feature mappings and migration
from removed Core options.
