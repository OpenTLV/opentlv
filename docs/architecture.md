# Library architecture and public C API

The [Fixed TLV Foundation](https://github.com/OpenTLV/opentlv/issues/3)
defines a C99 interface for C and C++11 callers. Include `<opentlv/tlv.h>`
to use the public macros for compiler feature detection and declaration
attributes.

## Modules and dependencies

Core owns format-independent representations, wire-format rules and shared
result codes. IO depends on Core. Core can be used on its own. The fixed
format defines how to encode an element and determine its extent in a
buffer. The reader and writer use those rules and manage their own cursors.
Protocol-specific interpretation stays outside both modules.

| Foundation work | Public header location | Tracking issue |
| --- | --- | --- |
| Tag, length, value, fixed encoding and size bounds | `opentlv/core/` | [#5](https://github.com/OpenTLV/opentlv/issues/5) |
| Bounded input and read cursor | `opentlv/io/reader/` | [#6](https://github.com/OpenTLV/opentlv/issues/6) |
| Bounded output and write cursor | `opentlv/io/writer/` | [#7](https://github.com/OpenTLV/opentlv/issues/7) |
| Shared results and diagnostic details | `opentlv/core/` | [#8](https://github.com/OpenTLV/opentlv/issues/8) |

## Public and private interfaces

C99 is the implementation baseline. C++11 callers include the same C
headers. Each module header includes its prerequisites and has an
`OPENTLV_…_H` include guard. Headers declaring external functions wrap those
declarations in `extern "C"` under `#ifdef __cplusplus`. Compile the
implementation as C and link it using a compatible C ABI.

Use `tlv_` for public C functions, `tlv_…_t` for public typedefs and
`TLV_` for result constants and feature macros. The umbrella header exposes
`TLV_C_VERSION` and three portable attributes. Mark functions whose results
should be checked with `TLV_NODISCARD`, intentionally unused declarations
with `TLV_MAYBE_UNUSED`, and intentional fallthrough between switch cases
with `TLV_FALLTHROUGH;`. OpenTLV reserves all three attribute names.
Predefining any of them before including the header is a compilation error.

Expose `include/` as the consumer include path. Keep each module's public
headers under `include/opentlv/<module>/`, implementations in
`src/<module>/`, and shared private headers in `src/<module>/internal/`.
Public headers may include standard headers and public dependencies.
Use `static` for file-local helpers. For shared-library builds, only
public symbols receive the owning module's `OPENTLV_<MODULE>_API` export
annotation. Private headers and symbols are outside the public API.

## Ownership contract

The reader borrows its input from the caller. Copying a borrowed tag,
value or element copies only its descriptor. Keep the backing bytes alive
and unchanged until the last use of any view, even after the reader goes
out of scope. The writer borrows a writable buffer whose capacity and
lifetime the caller controls.

The planned value type, `tlv_value_t`, pairs a read-only byte pointer with
a logical byte count (`tlv_size_t`, a 64-bit unsigned integer). Before
using a logical count to access memory, check that it fits the native
`size_t` and the actual buffer. Public declarations document borrowing,
null and empty inputs, output state on failure, and any invalidation
rules. The concrete types and their boundary tests belong to #5.

## Compatibility

Header paths, function signatures, public type layouts and documented
behavior form the public contract. Before 1.0, intentional breaking changes
must be identified in the pull request and changelog, with migration guidance
for callers. From 1.0 onward, preserve source compatibility and the public
C ABI within a major version on the same supported platform and ABI.
Removing an exported function or changing a public struct's layout requires
a new major version.

## Verification

Run `python tests/check_public_api.py` from the repository root with Python
3 and a GCC- or Clang-compatible toolchain. Set `CC` and `CXX` to select
the C and C++ compiler executables. To use Clang's Microsoft-compatible
driver, run `python tests/check_public_api.py --clang-cl clang-cl`.
The check compiles every public header in isolation and exercises the
attribute diagnostics in C99, C11, C17, C23, C++11 and C++17. C99 and
C++11 are required. The script reports newer modes the compiler cannot run.

The epic tracks installed-package and link checks in
[#9](https://github.com/OpenTLV/opentlv/issues/9), behavioral test and
coverage infrastructure in [#10](https://github.com/OpenTLV/opentlv/issues/10),
CI workflows in [#11](https://github.com/OpenTLV/opentlv/issues/11), and
the usage guide and runnable examples in
[#12](https://github.com/OpenTLV/opentlv/issues/12).
