# Core and IO API and ABI baseline for v0.1.0

`OpenTLV::core` contains the fixed-format primitives. Its sources and public
headers require only Core and the standard library. `OpenTLV::io` links Core
publicly and adds the reader and writer. A Core-only consumer can include
`<opentlv/core/formats/fixed.h>` or its `.hpp` counterpart and link Core alone.
The umbrella headers `<opentlv/tlv.h>` and `<opentlv/tlv.hpp>` expose both modules.

## C99 binary interface

The [Core](../tests/abi/core.h) and [IO](../tests/abi/io.h) inventories
record the names, parameter types and return types of all 20 external
functions. Consumers include the [public headers](../include/opentlv).

| Library | Functions |
| --- | --- |
| Core (10) | Two result lookups, two checked size operations, value initialization and validation, fixed-length read and write, header and element measurement |
| IO (10) | One-shot read and write, reader and writer initialization, cursor read and write, and diagnostic variants of all four read and write operations |

The C records below are caller-allocated. Member order matters even when a
function takes its record by pointer. For example, an application allocates a
`tlv_reader_t` and the library writes its cursor state into that storage.

| C record | Members in declaration order |
| --- | --- |
| `tlv_tag_t`, `tlv_length_t` | `const uint8_t *data`, `size_t size` |
| `tlv_value_t` | `const uint8_t *data`, `tlv_size_t size` |
| `tlv_element_t` | `tlv_tag_t tag`, `tlv_value_t value` |
| `tlv_fixed_identifier_t` | `size_t size` |
| `tlv_fixed_length_t` | `size_t size`, `tlv_byte_order_t byte_order` |
| `tlv_fixed_format_t` | `tlv_fixed_identifier_t identifier`, `tlv_fixed_length_t length` |
| `tlv_diagnostic_t` | `tlv_result_t result`, `tlv_field_t field`, `size_t offset`, `uintmax_t required`, `uintmax_t available` |
| `tlv_reader_t` | `const uint8_t *data`, `size_t size`, `size_t pos`, `tlv_fixed_format_t format` |
| `tlv_writer_t` | `uint8_t *data`, `size_t capacity`, `size_t pos`, `tlv_fixed_format_t format` |

The four C enums also participate in the ABI. The values below include the
gaps in the result codes.

| C enum | Enumerators and values |
| --- | --- |
| `tlv_result_t` | `TLV_OK` = 0, `TLV_END` = 1. With prefix `TLV_ERR_`, `TRUNCATED` = 3, `INVALID_TAG_SIZE` = 5, `INVALID_LENGTH` = 6, `NULL_ARG` = 10, `INVALID_ARG` = 11, `BUFFER_TOO_SHORT` = 14, `OVERFLOW` = 16, `NATIVE_SIZE` = 17. |
| `tlv_result_category_t` | With prefix `TLV_CATEGORY_`, `SUCCESS`, `END`, `INVALID_ARGUMENT`, `MALFORMED_DATA`, `TRUNCATED`, `CAPACITY`, `RANGE`, `UNKNOWN` have values 0 through 7 in order. |
| `tlv_byte_order_t` | With prefix `TLV_BYTE_ORDER_`, `UNKNOWN`, `BIG_ENDIAN`, `LITTLE_ENDIAN` have values 0 through 2 in order. |
| `tlv_field_t` | With prefix `TLV_FIELD_`, `NONE`, `TAG`, `LENGTH`, `VALUE`, `ELEMENT` have values 0 through 4 in order. |

The library and caller must agree on the target C ABI, including the calling
convention, pointer and `size_t` widths, enum representation, alignment and
structure packing. `tlv_size_t` is `uint64_t`, with maximum `TLV_SIZE_MAX`.
Diagnostic counts use `uintmax_t`. The [layout baseline](../tests/abi_layout.cpp)
records native sizes, alignments and member offsets for x86-64 with eight-byte
pointers and for Win32 with MSVC packing. On the wire, the chosen tag width,
length width and byte order determine the encoding independently of native
struct layout. See the [fixed format](fixed-tlv.md).

The C source interface also includes the language and feature macros
`TLV_C_VERSION`, `TLV_C99`, `TLV_C11`, `TLV_C17`, `TLV_C23`,
`TLV_HAS_C11`, `TLV_HAS_C17`, `TLV_HAS_C23`, `TLV_HAS_ATTRIBUTE`,
`TLV_HAS_BUILTIN` and `TLV_HAS_DECLSPEC_ATTRIBUTE`. The declaration and
statement attributes are `TLV_NODISCARD`, `TLV_MAYBE_UNUSED` and `TLV_FALLTHROUGH`.

## C++17 source interface

Consumers compile the inline C++ operations from the installed headers.
The wrappers access the compiled library through functions with C linkage
and pass native C types across that boundary.

| Namespace | Public types | Public operations |
| --- | --- | --- |
| `opentlv::core` | `byte_view`, `mutable_byte_view`, `tag_view`, `length_view`, `value_view`, `element_view` | `make_value_view`, `validate` |
| `opentlv::core` | `logical_size` | `size_to_native`, `size_add` |
| `opentlv::core` | `byte_order`, `fixed_identifier`, `fixed_length`, `fixed_format` | `read_length`, `write_length`, `header_size`, `element_size` |
| `opentlv::core` | `result`, `category`, `field`, `diagnostic` | `result_category`, `result_string` |
| `opentlv::io` | `reader` | Free `read`. Members `init`, `next`, `position`, `format`. |
| `opentlv::io` | `writer` | Free `write`. Members `init`, `write`, `position`, `format`. |

`opentlv::core::to_c` and `from_c` provide field-by-field or enum conversions
for the 12 C/C++ type pairs in the [interoperability table](cpp-api.md#types-and-c-interoperability).
Both cursors are default-constructible. Copies of a reader have independent
positions. A writer disables copy and move operations.

## Visibility and verification

Public C declarations use `OPENTLV_CORE_API` or `OPENTLV_IO_API`. For a
static build these expand to empty and archive members supply the external
definitions. Shared builds with GCC or Clang on Unix use hidden visibility
by default and mark public functions with default visibility. On Windows,
each DLL uses `dllexport` when defining its functions and its consumers use
`dllimport` through the CMake target. The libraries expose functions only.
Core implementation helpers have internal linkage. The IO reader and writer
share two `static inline` helpers in the private header
`src/io/internal/diagnostic.h`, outside the installed include tree.

The following Release builds each supplied the ten listed Core functions and
the ten listed IO functions in both static and shared libraries.

| Local target and toolchain | Additional observations |
| --- | --- |
| Windows x64, MinGW-w64 GCC 16.2 and MSVC 19.52 | C99 and C++17 consumers compiled and ran against the build tree and the relocated installation. |
| Windows x86, MSVC 19.52 | Archive symbols carry a leading underscore. The IO DLL imported Core. The Core DLL imported only Windows and C runtime libraries. |
| Linux x86-64, cross-compiled with Clang 18.1.6 for glibc and musl | The IO shared object listed Core in `DT_NEEDED`. Core had no IO dependency. The musl shared objects also exported `_init` and `_fini`, whose addresses matched the ELF loader's `DT_INIT` and `DT_FINI` entries. |

Run the header and package checks from the repository root. The symbol audit
supports Linux ELF and Windows PE/COFF. The [header check](architecture.md#verification)
uses GCC or Clang and also accepts `--clang-cl clang-cl` on Windows.

```sh
python tests/check_public_api.py
python -m unittest discover -s tests -p test_check_symbols.py
python tests/check_cmake_package.py --check-symbols
python tests/check_cmake_package.py --check-symbols --shared
```

Each header compiles in its own translation unit. Core headers and C sources
also compile with only Core available. The package checks verify which headers
were installed, compile each through its imported target, and check the public
link from IO to Core in both package locations.

The symbol check compares each library's static definitions or shared exports
with its inventory. For static archives, GNU `nm` reports externally defined
symbols. The MSVC check reads the archive's public symbol index and skips
compiler-generated entries for pooled string literals (`??_C@` COMDATs). For
shared ELF libraries, the check uses `objdump -p` to identify loader entries
by address and reports them separately. An extra symbol, a missing name, or
a function in the wrong library fails the comparison. Fixture tests exercise
the symbol parsers with saved ELF and PE/COFF tool output.

Both the library test build and the C++ package checks compile `abi-layout`.
Its C signature and enum assertions run at compile time on every target.
On x86-64 with eight-byte pointers and Win32 with MSVC-compatible packing,
the build also checks record sizes, alignments and member offsets. For other
targets CTest reports `abi-layout` as skipped. Run CTest in the build directory
with `-V -R "^abi-layout$"` to see the reason.

Review baseline changes alongside the [consumer tests](architecture.md#verification)
and the [compatibility policy](architecture.md#compatibility). For release
evidence, record the tested revision, target ABI, compiler and both package
results so the comparison can be repeated against the release artifacts.
