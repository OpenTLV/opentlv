# Build only the components you need

OpenTLV builds only the components you ask for. This guide gives ready-made CMake
recipes for the common selections. For the principle and the option reference, see
[include only what you need](../concepts/architecture.md#include-only-what-you-need) and
[build configuration](../concepts/architecture.md#build-configuration).

Core, IO, Document, Generator and Query are independent C libraries. Core owns Definition, Format and Field
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

`OPENTLV_READER`, `OPENTLV_WRITER`, `OPENTLV_DOCUMENT` and `OPENTLV_QUERY` are no longer CMake
options. Supplying any of them
is an error; remove them from presets and existing caches (or use a fresh build
directory). IO always includes both APIs. For transition compatibility,
`tlv/config.h` retains the constant macros with value 1 and their runtime mirrors;
they do not select sources or indicate which modules an application linked.

Existing CMake consumers of `OpenTLV::tlv` and `OpenTLV::tlv++` receive Core, IO,
Document, Generator, Query and QueryDocument transitively. Direct library consumers must link the dependencies too (static
link order: `tlv`, `opentlv_query_document`, `opentlv_query`, `opentlv_document`, `opentlv_generator`, `opentlv_io`, `opentlv_core`). Shared deployments must distribute
all linked libraries. Rebuild consumers after the split; the old single-library
binary distribution is not preserved.

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

## Remaining capability profiles

Schema, Codec and builtin selection remain transitional options until their own
extraction issues. The compatibility target includes Document, Generator, Query
and IO in every profile. Generated headers must match the linked libraries.

| Profile | Schema | Codec |
| --- | --- | --- |
| IO, Document, Query or Document/Query | OFF | OFF |
| Schema | ON | OFF |
| Codec | OFF | ON |
| Full | ON | ON |

Broad unit/integration suites need the full profile. Module C tests and the
`capability-contract` remain available in reduced builds.
`python scripts/check_capabilities.py` validates these remaining profiles;
`scripts/check_module_isolation.py` separately builds each extracted module in fresh trees,
checks symbol/header boundaries and runs relocated package consumers. CI runs
static and shared variants of both checks.

Standalone bindings and tools own their Core dependency checks and pins. Their
build scripts must migrate removed CMake options when adopting this revision.

## Generic core formats

| Format | Availability |
| --- | --- |
| [Configurable fixed-width TLV](../formats/fixed/configurable.md) | Always built; shared C implementation and C++ wrapper |

## Built-in standards

| Option | Component |
| --- | --- |
| `OPENTLV_NFC` | [NFC Type 2 Tag framing](../formats/nfc/README.md) for contiguous TLV streams |
| `OPENTLV_DHCP` | [DHCPv4 option framing](../formats/dhcp/README.md), including Pad and End |
| `OPENTLV_BLUETOOTH` | [Bluetooth](../formats/bluetooth/README.md): LTV format, containers, definitions, schemas and codecs |
| `OPENTLV_LLDP` | [LLDP](../formats/lldp/README.md): packed framing and base Type definitions; no schemas or value codecs |
| `OPENTLV_FORMAT_ASN1` | The ASN.1 group; must be ON for BER, DER, CER and EMV |
| `OPENTLV_FORMAT_BER` | [BER-TLV](../formats/asn1/ber.md); must be ON for DER, CER and EMV |
| `OPENTLV_FORMAT_DER` | [DER-TLV](../formats/asn1/der.md); independent of EMV |
| `OPENTLV_FORMAT_CER` | [CER-TLV](../formats/asn1/cer.md); independent of DER |
| `OPENTLV_EMV` | [EMV module](../standards/emv/README.md) |

The [mutable document](document.md) is a separate optional generic component,
selected by linking `OpenTLV::Document`; it is not a format or a standard package.

Turning an option OFF also turns OFF everything below it in the chain
`ASN1 -> BER`, with DER, CER and EMV as siblings under BER.
EMV framing uses generic primitives; only the unchanged DOL implementation
retains a BER dependency. EMV does not require DER or CER.
The C++ [configurable fixed-width format](../formats/fixed/configurable.md)
template delegates to the always-built C implementation.

## Recipes

Each recipe starts from a clean build directory; replace `build` with your own. Every
option defaults to ON, so a recipe lists exactly the options it turns OFF.

### Generic formats without protocol extensions

Use this when you supply your own [format callbacks](../formats/custom/README.md).

```sh
cmake -S . -B build \
  -DOPENTLV_BLUETOOTH=OFF -DOPENTLV_LLDP=OFF -DOPENTLV_DHCP=OFF -DOPENTLV_NFC=OFF -DOPENTLV_FORMAT_ASN1=OFF
cmake --build build --parallel --target tlv
```

### Core and BER

```sh
cmake -S . -B build \
  -DOPENTLV_BLUETOOTH=OFF -DOPENTLV_LLDP=OFF -DOPENTLV_DHCP=OFF -DOPENTLV_NFC=OFF \
  -DOPENTLV_FORMAT_DER=OFF -DOPENTLV_FORMAT_CER=OFF -DOPENTLV_EMV=OFF
cmake --build build --parallel --target tlv
```

### Core, BER, DER and EMV

This recipe enables DER alongside EMV. EMV itself does not need DER;
add `-DOPENTLV_FORMAT_DER=OFF` for BER and EMV only. CER is off.

```sh
cmake -S . -B build \
  -DOPENTLV_BLUETOOTH=OFF -DOPENTLV_LLDP=OFF -DOPENTLV_DHCP=OFF -DOPENTLV_NFC=OFF -DOPENTLV_FORMAT_CER=OFF
cmake --build build --parallel --target tlv
```

### Core and Bluetooth LTV only

Bluetooth uses the generic binary composition primitives. Fixed is always available.

```sh
cmake -S . -B build \
  -DOPENTLV_FORMAT_ASN1=OFF -DOPENTLV_LLDP=OFF -DOPENTLV_DHCP=OFF -DOPENTLV_NFC=OFF
cmake --build build --parallel --target tlv
```

### Everything

The default. No options are needed.

```sh
cmake -S . -B build
cmake --build build --parallel
```

## What you get

Fixed remains available in every configuration, including core-only builds.
Disabled protocol extensions contribute no implementation symbols.

Confirm your own selection from the generated `tlv/config.h`, which defines each option
as `1` or `0`:

```c
#include "tlv/config.h"

#if OPENTLV_FORMAT_BER
#include "tlv/builtins/asn1/ber.h"
/* use tlv_format_ber */
#endif
```

Use these macros when your code has to compile against a reduced build. Including the
header of a disabled component does not provide its symbols.

## Notes

- Tests, examples and the CLI that need a disabled component are omitted from that build.
- The C++ wrapper adds cost only for the headers you include.
- Rust exposes LLDP through the default `lldp` feature; `--no-default-features`
  disables that package in source builds. Other C components retain their defaults. See
  [include only what you need](../concepts/architecture.md#include-only-what-you-need).
- Protocol extensions remain optional; generic formats belong to the core. See the
  [format expansion candidates](../formats/format-roadmap.md).
