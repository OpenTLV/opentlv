# Build only the components you need

OpenTLV builds only the components you ask for. This guide gives ready-made CMake
recipes for the common selections. For the principle and the option reference, see
[include only what you need](../concepts/architecture.md#include-only-what-you-need) and
[build configuration](../concepts/architecture.md#build-configuration).

Core and IO are independent C libraries. Core owns Definition, Format and Field
Encoding, Element and Layout, byte utilities and generic diagnostics. IO owns
Reader, Writer, traversal and serialization and depends publicly on Core.

## Core and IO

Select functionality by linking the target you use:

```cmake
find_package(OpenTLV REQUIRED CONFIG COMPONENTS Core IO)
target_link_libraries(my_parser PRIVATE OpenTLV::IO)
# For generic primitives without Reader/Writer:
target_link_libraries(my_format_tool PRIVATE OpenTLV::Core)
```

The same targets are available through `add_subdirectory()` and FetchContent.
A Core-only package consumer can request just `COMPONENTS Core`; no IO or C++
library is loaded. Both modules support static and shared builds, selected by
`OPENTLV_BUILD_SHARED_LIBS`. A library-only build does not require a C++ compiler,
even when the compatibility C++ facade is enabled.

Build only the required target from a checkout:

```sh
cmake -S . -B build/modules -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build/modules --target opentlv_core
# Builds Core and IO, without the remaining library implementations:
cmake --build build/modules --target opentlv_io
```

`OPENTLV_READER` and `OPENTLV_WRITER` are no longer CMake options. Supplying either
is an error; remove them from presets and existing caches (or use a fresh build
directory). IO always includes both APIs. For transition compatibility,
`tlv/config.h` retains the constant macros with value 1 and their runtime mirrors;
they do not select sources or indicate which modules an application linked.

Existing CMake consumers of `OpenTLV::tlv` and `OpenTLV::tlv++` receive Core and IO
transitively. Direct library consumers must link the dependencies too (static
link order: `tlv`, `opentlv_io`, `opentlv_core`). Shared deployments must distribute
all linked libraries. Rebuild consumers after the split; the old single-library
binary distribution is not preserved.

Public Reader/Writer headers and function signatures remain at their existing
paths. `tlv_copy_element()` now belongs to `tlv/writer/copy.h`; Core `tlv/copy.h`
contains raw byte/value copies only. `tlv/tlv.h` includes both. New public utility
headers are `tlv/utf8.h`, `tlv/callback.h` and `tlv/diagnostic/cause.h`. Module
exports use `OPENTLV_CORE_API` and `OPENTLV_IO_API`; CMake propagates the matching
static-build definitions automatically.

## Remaining capability profiles

Document, Query, Schema, Codec and builtin selection remain transitional options
until their own extraction issues. The compatibility target includes IO in every
profile. `OPENTLV_QUERY=OFF` also disables its frontend and set operations.
Generated headers must match the linked libraries.

| Profile | Document | Query | Schema | Codec |
| --- | --- | --- | --- | --- |
| IO | OFF | OFF | OFF | OFF |
| Document | ON | OFF | OFF | OFF |
| Query | OFF | ON | OFF | OFF |
| Document/Query | ON | ON | OFF | OFF |
| Schema | OFF | OFF | ON | OFF |
| Codec | OFF | OFF | OFF | ON |
| Full | ON | ON | ON | ON |

Broad unit/integration suites need the full profile. Module C tests and the
`capability-contract` remain available in reduced builds.
`python scripts/check_capabilities.py` validates these remaining profiles;
`scripts/check_module_isolation.py` separately builds Core and IO in fresh trees,
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
controlled by `OPENTLV_DOCUMENT`; it is not a format or a standard package.

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
