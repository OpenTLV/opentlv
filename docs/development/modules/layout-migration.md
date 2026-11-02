# Module-first header migration

Issue [#733](https://github.com/OpenTLV/opentlv/issues/733) moves the OpenTLV
Library to a functional layout after #732. The maintainer approved the
`opentlv/<module>/` and `opentlv/builtin/<family>/` naming for this migration
on 2026-11-02, coordinating the header-path portion of #702. This does not
approve or complete the other ecosystem naming changes in #702.

## Public includes

C `.h` and header-only C++ `.hpp` files share their owning module directory.
The [complete path map](header-paths.json) includes generated public headers.
Update include directives and rebuild consumers; there are no forwarding
headers at the old paths. C/C++ symbols, namespace `tlv`, CMake target names,
wire behavior and diagnostics are unchanged by this migration.

| Previous include | Current include | Target |
| --- | --- | --- |
| `tlv/format.h` | `opentlv/core/format.h` | `OpenTLV::Core` |
| `tlv++/format.hpp` | `opentlv/core/format.hpp` | `OpenTLV::Core` |
| `tlv/reader/reader.h` | `opentlv/io/reader/reader.h` | `OpenTLV::IO` |
| `tlv++/document/document.hpp` | `opentlv/document/document.hpp` | `OpenTLV::Document` |
| `tlv++/document/diff.hpp` | `opentlv/query/diff.hpp` | `OpenTLV::QueryDocument` |
| `tlv/builtins/asn1/ber.h` | `opentlv/builtin/asn1/ber.h` | `OpenTLV::ASN1` |
| `tlv/version.h` | `opentlv/core/version.h` | `OpenTLV::Core` |

Integration headers live with their owning module: QueryDocument and QueryCodec
under `query/`, DocumentCodec under `document/`, and IOCodec and CodecStructure
under `codec/`. Generated integration export headers use a subdirectory, such as
`opentlv/query/document/export.h`. The optional aggregates are `opentlv/tlv.h`,
`opentlv/tlv.hpp` and the explicit interop header `opentlv/native.hpp`.

Pure-C consumers can still use `project(app LANGUAGES C)` and functional targets.
`OPENTLV_BUILD_CXX=OFF` disables C++ development targets, without removing installed
C++ headers. See [component selection](../../guides/select-components.md).
External binding and tool migrations belong to their respective repositories.

## Implementation ownership

```text
include/opentlv/<module>/          public C and C++ headers
include/opentlv/builtin/<family>/  public builtin headers
src/<module>/                     C implementations and source templates
src/<module>/internal/            owner-private headers and header templates
src/builtin/<family>/             builtin implementations
cmake/library/                    target and facade registrations
```

There is no shared internal target or cross-module private-header exception.
All six builtin targets depend only on generic module targets. EMV DOL's
identifier parser now configures the existing Core variable-identifier primitive
with its historical BER acceptance rules, removing the former ASN.1 dependency.
An exhaustive two-byte-prefix comparison and boundary cases verify equivalence
to the previous BER reader. The remaining DOL composition work stays in #695.
Header-only C++ `detail/` helpers must be installed because public templates use
them; they remain implementation details of their facade, subject to the C++
consumer boundary check. They are distinct from uninstalled C private headers.

`check_module_contracts.py` resolves quoted and angle includes, including relative
paths, before checking ownership; macro include operands are rejected because
they would hide ownership from the scan. Public headers cannot expose private headers;
builtins cannot include generic private headers even through a declared target
dependency. The Static Analysis job runs the checker and its negative tests.
`check_module_isolation.py` also verifies configured target ownership, symbol
boundaries, isolated builds, relocatable installations and individual installed
C/C++ headers for static and shared builds.

This migration preserves the existing clang-tidy configuration selection and
header-filter path spelling. Normalizing the historical `cmake/../` filter and
enabling the optional C++ modernize profile exposes existing header findings;
that broader analysis needs a separate cleanup. Standalone header compilation
and cppcheck's direct C++ header scan remain separate checks.

## Review and final verification

Keep the pure `git mv` changes in a separate commit from the include, build and
tooling rewrite. Keep the DOL dependency change and its regression test separate
from the bulk mechanical rewrite as well. After creating the mechanical rewrite
commit, append its full hash to `.git-blame-ignore-revs` in a subsequent commit.
A move-only commit does not need to be ignored. No placeholder hash should be
recorded.

For #655, retain static/shared CTest results, pure-C and C++ installed consumers,
standalone headers, isolation checks and replay of the immutable #645 baseline.
The baseline files and historical ABI snapshot must remain unchanged. The probe
tools adapt current include directives when compiling against historical source
archives; they do not install compatibility headers. Full hosted CI and the
final #655 evidence remain separate from local validation.
