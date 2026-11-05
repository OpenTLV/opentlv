# C source amalgamation contract

This page is the target composition and public artifact contract for the C
source amalgamation of the OpenTLV Library. It is the deliverable of
[#741](https://github.com/OpenTLV/opentlv/issues/741) for epic
[#701](https://github.com/OpenTLV/opentlv/issues/701). It is written for the
contributors who implement the generator
([#740](https://github.com/OpenTLV/opentlv/issues/740)), the install component
and consumers ([#742](https://github.com/OpenTLV/opentlv/issues/742)), the
equivalence gate ([#743](https://github.com/OpenTLV/opentlv/issues/743)) and the
release artifacts ([#744](https://github.com/OpenTLV/opentlv/issues/744)), and
for the reviewers who approve it. Read the
[functional module contracts](modules/README.md) and the
[Library and Core naming contract](naming.md) first.

**Status:** contract version 1, implemented by the `opentlv_amalgamation` build
target ([#740](https://github.com/OpenTLV/opentlv/issues/740); see
[Generation](#generation)), the optional install component (#742), and the
[distribution equivalence gate](#distribution-equivalence-gate) (#743).
Publication of a release asset remains tracked by #744.

An *amalgamation* is a generated C99 header/source pair containing the
Library's modular C implementation, so that a consumer can compile OpenTLV
without CMake. It is a distribution form of the Library, not another
implementation. A *conventional build* is the CMake build of the module
libraries from the same commit and configuration.

## Target inventory

The module registry in [`cmake/library/modules.cmake`](../../cmake/library/modules.cmake)
is authoritative for targets, sources, headers and dependencies, and
[ownership.json](modules/ownership.json) is authoritative for ownership. The
table below summarizes the registry of commit `04e637aa` (Library v0.8.0 plus
55 commits) with both Query features enabled, in registration order. Counts are
evidence for this review and are not maintained; the generator reads the
registry.

| Key | Target | Kind | C sources | Public C headers | Private headers | C++ headers | Public deps | Private deps |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| `core` | `OpenTLV::Core` | module | 21 (1 generated) | 26 | 3 | 12 | - | - |
| `io` | `OpenTLV::IO` | module | 7 | 9 | 0 | 9 | core | - |
| `generator` | `OpenTLV::Generator` | module | 1 | 1 | 0 | 1 | core | io |
| `document` | `OpenTLV::Document` | module | 3 | 2 | 1 | 1 | core, io | - |
| `query` | `OpenTLV::Query` | module | 8 | 4 | 4 | 5 | core, io | - |
| `query-document` | `OpenTLV::QueryDocument` | integration (Query) | 2 | 1 | 0 | 2 | core, io, document, query | - |
| `codec` | `OpenTLV::Codec` | module | 8 | 7 | 1 | 5 | core | - |
| `query-codec` | `OpenTLV::QueryCodec` | integration (Query) | 1 | 1 | 0 | 1 | core, io, query, codec | - |
| `schema` | `OpenTLV::Schema` | module | 5 | 3 | 2 | 4 | core, io | - |
| `schema-number` | `OpenTLV::SchemaNumber` | integration (Schema) | 1 | 1 | 0 | 0 | schema, codec | core |
| `schema-query` | `OpenTLV::SchemaQuery` | integration (Schema) | 1 | 1 | 2 | 0 | core, io, schema, query | - |
| `schema-query-document` | `OpenTLV::SchemaQueryDocument` | integration (Schema) | 1 | 1 | 0 | 0 | core, io, schema, document, schema-query, query, query-document | - |
| `codec-structure` | `OpenTLV::CodecStructure` | integration (Codec) | 1 | 1 | 0 | 1 | core, codec, schema | io |
| `asn1` | `OpenTLV::ASN1` | builtin | 13 | 9 | 5 | 6 | core, io, codec, schema, query | - |
| `emv` | `OpenTLV::EMV` | builtin | 8 | 7 | 2 | 4 | core, codec, schema, query | schema-number |
| `bluetooth` | `OpenTLV::Bluetooth` | builtin | 9 | 9 | 0 | 3 | core, io, codec, schema | - |
| `nfc` | `OpenTLV::NFC` | builtin | 1 | 1 | 0 | 1 | core | - |
| `dhcp` | `OpenTLV::DHCP` | builtin | 3 | 4 | 0 | 3 | core, codec, schema | io |
| `lldp` | `OpenTLV::LLDP` | builtin | 3 | 3 | 0 | 2 | core, codec, schema | io |
| `document-codec` | `OpenTLV::DocumentCodec` | C++ facade | 0 | 0 | 0 | 1 | document, codec | - |
| `io-codec` | `OpenTLV::IOCodec` | C++ facade | 0 | 0 | 0 | 1 | io, codec | - |
| **Total** | | | **97** | **91** | **20** | **60** | | |

The 19 compiled targets contribute C. The two facade-only targets and every
`.hpp` file contribute no C. The aggregate `OpenTLV::tlv` is not registered. It
owns the C umbrella `opentlv/tlv.h`, which contains only includes and is not an
input of `opentlv.h`, and the C++ headers `opentlv/tlv.hpp` and
`opentlv/native.hpp`, which ship unchanged in the `include/` tree.

Configuration produces these generated inputs:

| Generated file | Owner | Produced by | Amalgamation form |
| --- | --- | --- | --- |
| `opentlv/<path>/export.h` (one per compiled target, 19) | each target | CMake `GenerateExportHeader` | Replaced by the portable export block below; CMake's compiler-specific text is not copied |
| `opentlv/core/version.h`, `src/version.c` | Core | `cmake/version.cmake` templates | Copied from the configured output |
| `opentlv/query/features.h` | Query | `src/query/internal/query-features.h.in` | Copied from the configured output |

`version.h` and `features.h` are installed public headers. At commit `04e637aa`
no target listed them, because `opentlv_add_module()` accepted public headers
only below `INCLUDE_DIR`. `features.h` is reachable from `opentlv/query/query.h`;
`version.h` is reachable from no public C header, only from
`opentlv/core/version.hpp` and `version.c`. Since #740, Core and Query register
them as `GENERATED_HEADERS`, so that composition reads them from the registry
instead of special-casing file names, and the registration installs them.

## Artifact composition

Version 1 publishes one self-contained composition: every compiled target in
the registry, for one configuration. Per-module artifacts, custom or minimal
module selection, and transitive per-target composition are out of scope until
a later contract version. A consumer that needs fewer modules links the
conventional module libraries.

| File | Contents | Consumers |
| --- | --- | --- |
| `opentlv.h` | Every public C header of the 19 compiled targets and the generated headers, flattened | C99 consumers |
| `opentlv.c` | `#include "opentlv.h"`, then every C source and private header of the 19 compiled targets, flattened | Compiled once, as C, into the consumer |
| `include/` | The installed public C and C++ header tree, unmodified, with the generated headers in their amalgamation form | C++ facade consumers and consumers using module include paths |
| `opentlv-amalgamation.json` | Configuration and input metadata; see [Metadata](#metadata) | Equivalence checks and verifiers |

`opentlv.c` includes only `opentlv.h` and C99 standard headers. The `include/`
tree is needed because the header-only C++ facade includes the modular C
headers by path (`#include "opentlv/core/tag.h"`). Its generated headers are
byte-identical to the corresponding blocks of `opentlv.h` and keep the same
include guards, so a translation unit may include both.

## Generator inputs and authority

- The registry is the only source of targets, sources, headers, dependencies
  and their order. The generator does not scan directories, parse
  `ownership.json` for composition or keep its own list of files.
- `opentlv_finalize_modules()` already writes the per-configuration
  `modules-<config>.json`. CMake 3.16, the Library's minimum, cannot parse JSON
  (`string(JSON)` needs 3.19), so the generator reads a registry inventory
  written in CMake syntax by the same finalization, like the installed
  `OpenTLVModules.cmake`: `<build>/opentlv-registry.cmake`. Generation needs no
  CMake newer than 3.16 and no Python.
- Generation is the explicit build target `opentlv_amalgamation`, which runs a
  `cmake -P` script. It is not part of `ALL`, and `install()` never generates.
  The target exists only in standalone Library builds; embedding builds do not
  receive it.
- Source selection by a Query feature follows the registry: a feature-disabled
  source is absent from `SOURCES` and therefore from the amalgamation.
- The only compile definitions the registry applies to module targets are
  `OPENTLV_<KEY>_STATIC_DEFINE`, which the portable export block makes
  unnecessary. Any other compile definition, compile option with semantic
  effect or new generated input fails generation until this contract covers it.

## Composition order

The order is a function of the registry alone:

1. Targets follow registration order (`OPENTLV_MODULES`). That order must be a
   topological order of the public and private dependencies; it is today. The
   generator fails instead of reordering when a target precedes one of its
   dependencies.
2. `opentlv.h` starts with the prologue described in [Notices](#notices), then
   visits each target's registered public C headers in registered order,
   followed by its generated public headers in registered order.
3. `opentlv.c` starts with the prologue and `#include "opentlv.h"`, then visits
   each target's `SOURCES` in registered order. The generated `version.c` keeps
   its registered position in Core.
4. A visited file is emitted depth-first: a quoted `#include` of a Library
   header is replaced by that header's text at its first inclusion and removed
   at later ones. Public headers are emitted only in `opentlv.h`; in
   `opentlv.c` their includes are removed. Private headers are emitted in
   `opentlv.c` at their first inclusion.
5. Each emitted file is delimited by begin and end comments naming its path
   relative to the source root, or `generated/<path>` for generated inputs:
   `/* ==== begin src/core/tag.c ==== */` and `/* ==== end src/core/tag.c ==== */`.
   A removed include leaves no line behind.

Facade-only targets are skipped. Ties cannot occur, because every position
comes from a registered list.

## Headers and includes

- Quoted includes resolve as the compiler resolves them: relative to the
  including file first, then the public include directory and the generated
  include directory. An include that resolves outside the registered headers of
  the composed targets fails generation. A private header may be reached only
  from sources of its owning module or its owner's integrations, as the module
  contract already requires.
- Angle includes are kept verbatim in place and must name C99 standard headers.
  The C sources and headers currently use `assert.h`, `ctype.h`, `limits.h`,
  `stdbool.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h` and `string.h`.
- Every header keeps its own include guard. Guards are unique across `include/`,
  `src/` and the generated headers, and no header uses `#pragma once`. A
  duplicate guard or `#pragma once` fails generation.
- No Library include is conditional today. A Library include inside a
  preprocessor conditional fails generation, because first-inclusion
  deduplication would depend on the condition.
- Public headers keep their `extern "C"` blocks, so `opentlv.h` declares every
  function with C linkage in C++ translation units.
- Text is copied unchanged apart from include replacement. No identifier is
  renamed, no code is added or removed, and no `#line` directives are emitted in
  version 1.

## Export macros, linkage and symbols

The amalgamation is compiled into the consumer, so it defines no import or
export attributes. For each compiled target the export block is:

```c
#ifndef OPENTLV_CORE_API_H
#define OPENTLV_CORE_API_H
#ifndef OPENTLV_CORE_API
#define OPENTLV_CORE_API
#endif
#endif
```

with the key changed per target. The guard matches the CMake-generated header.
Only `OPENTLV_<KEY>_API` is used by Library headers; the CMake `NO_EXPORT` and
`DEPRECATED` variants are not. A consumer that builds its own shared library
from the amalgamation may predefine these macros. Such a library is
consumer-owned and carries no ABI promise.

The amalgamation adds no public identifier: no amalgamation detection macro,
no new function and no `OPENTLV_API` hook. Public C++ wrappers therefore cannot
branch on the distribution form.

Every function or object with external linkage keeps it. The external symbols
of `opentlv.o` must equal the union of the external symbols of the static module
archives of the equivalent conventional build. At this revision that is 691
symbols: 623 public, 12 owner-internal and 56 other module-internal symbols.
Hidden visibility in shared modules has no counterpart in the amalgamation; the
internal symbols are external there, as they already are to static consumers.

## Configuration and platform-specific code

| Input | Contract |
| --- | --- |
| Query features | `OPENTLV_QUERY_FRONTEND` and `OPENTLV_QUERY_SET_OPERATIONS` are fixed at generation by the copied `features.h` and recorded in the metadata. Redefining them when compiling the amalgamation is unsupported. The published artifact uses the release defaults (both `1`). |
| Version metadata | `version.h` and `version.c` carry the version, revision, commit, branch and tag computed by `cmake/version.cmake`. They are inputs: two generations are identical only when these values are identical. |
| Linkage and language options | `OPENTLV_BUILD_SHARED_LIBS`, `OPENTLV_BUILD_CXX`, build type, generator and test, example or benchmark options do not affect the output. |
| Platform branches | No source or header branches on operating system or compiler identity. The only preprocessor feature tests are the `__cplusplus` linkage guards, `__STDC_VERSION__` and `__has_attribute`/`__has_builtin`/`__has_declspec_attribute` in `opentlv/core/compiler.h`, and the `SIZE_MAX > INT64_MAX` data-model test in `query/execute.c`. They evaluate in the consumer's compiler as in a conventional build. A new operating-system or compiler branch needs a review of this contract. |
| `NDEBUG` | `query/document_backend.c` runs a debug-only consistency check unless `NDEBUG` is defined, so behavior comparisons use the same `NDEBUG` setting as the conventional build they compare with (CMake `Release` defines it). |

## Source requirements and findings

The generator enforces these source rules and fails instead of rewriting code.
The Library fixes violations by source organization in #740, before or with
the generator.

1. Identifiers with internal linkage (file-scope `static` functions and
   objects, including `static inline` functions in headers) are unique across
   all composed sources and headers.
2. Types, struct, union and enum tags defined in a `.c` file are unique across
   all composed sources.
3. A macro defined in a `.c` file is `#undef`-ed before the end of that file.
4. Sources do not use `__FILE__`, `__DATE__`, `__TIME__`, `__COUNTER__` or
   `__BASE_FILE__`. None does today.
5. Inputs are UTF-8. Five ASN.1 files contain non-ASCII characters (`§`) in
   comments only.

The generator enforces rules 3 to 5. Rules 1 and 2 need a compiler: the
`amalgamation-c99` test compiles `opentlv.c` as one C99 translation unit, and a
violation fails that compile.

At commit `04e637aa`, including the registered sources in registration order
into one translation unit fails to compile as C99 with 56 errors. The
violations below were found by compiling each source separately, listing its
local symbols with `nm` and tracing each name to its defining files. The
compile gate of #740 remains authoritative for anything this inventory misses.
The generator issue resolved every finding below.

Internal-linkage names defined at more than one site (39 names, 94 definitions,
36 files, 30 of them across module boundaries); paths are relative to `src/`:

| Names | Definition sites |
| --- | --- |
| `decode_integer`, `encode_integer` | `builtin/asn1/asn1_codec.c`, `codec/values.c` |
| `read_length`, `read_tag`, `write_tag` | `builtin/asn1/ber.c`, `builtin/asn1/ber_internal.c`, `core/formats/escaped.c`, `core/formats/variable.c` |
| `read_value_bounds` | `builtin/asn1/ber.c`, `builtin/asn1/cer.c` |
| `fail` | `builtin/asn1/cer_validation.c`, `builtin/asn1/der_schema.c`, `builtin/asn1/der_validation.c`, `builtin/emv/dol.c` |
| `read_impl`, `traverse`, `visit_impl`, `write_impl` | `builtin/asn1/cer_validation.c`, `builtin/asn1/der_validation.c` |
| `unlocated` | `builtin/asn1/cer_validation.c`, `builtin/asn1/der_validation.c`, `builtin/emv/dol.c` |
| `identifiers` | `builtin/bluetooth/ad_schema.c`, `builtin/bluetooth/ad_types.c`, `builtin/bluetooth/company_ids.c`, `builtin/dhcp/options.c`, `builtin/lldp/lldp.c`, `builtin/lldp/schema.c` |
| `rules` | `builtin/bluetooth/ad_schema.c`, `builtin/lldp/schema.c` |
| `fields` | `builtin/bluetooth/ad_schema.c`, `builtin/lldp/schema.c`, `core/formats/escaped.c` |
| `definitions` | `builtin/bluetooth/ad_types.c`, `builtin/bluetooth/company_ids.c`, `builtin/dhcp/options.c` |
| `decode`, `encode` | `builtin/bluetooth/manufacturer_data.c`, `builtin/lldp/codec.c` |
| `validate_list` | `builtin/bluetooth/uuid.c`, `codec/ipv4.c` |
| `failure` | `builtin/dhcp/container.c`, `builtin/lldp/schema.c`, `core/formats/compose.c` |
| `codes`, `tag_only` | `builtin/dhcp/dhcpv4.c`, `builtin/nfc/type2.c` |
| `validate_length` | `builtin/emv/emv_codec.c`, `core/internal/field/escaped_internal.h` |
| `config` | `builtin/emv/format.c`, `builtin/nfc/type2.c` |
| `valid_config` | `codec/number.c`, `codec/text.c` |
| `write_length` | `core/formats/escaped.c`, `core/formats/variable.c` |
| `node_depth`, `subtree_size` | `document/document.c`, `query/document_backend.c` |
| `query_diag_init`, `query_error`, `query_error_unlocated`, `query_failure`, `query_input_location`, `query_limit`, `query_reader_failure`, `query_relate_expression`, `query_result_kind` | `query/internal/program_internal.h`, `schema/internal/query_diagnostic_internal.h` |
| `check_scope`, `same_tag` | `schema/report.c`, `schema/structure.c` |

Other findings:

- The generated public headers `opentlv/core/version.h` and
  `opentlv/query/features.h` are not registered (see
  [Target inventory](#target-inventory)). Without registration `opentlv.h`
  would omit the version API.
- `document_source_t` and `struct document_source` are defined in both
  `document/writer.c` and `query/document_backend.c` (rule 2).
- `EMV_COUNT` in `builtin/emv/emv.c` is not `#undef`-ed (rule 3). It collides
  with nothing today but would leak into every later source.
- The Schema copies of the Query diagnostic helpers in
  `schema/internal/query_diagnostic_internal.h` are deliberate: `schema-query`
  may use only the public Query API, so the fix is a Schema-owned name, not a
  shared private header.

Resolve rule 1 and 2 collisions by giving the helper a name owned by its file
or module, or by moving a helper that is identical within one module into that
module's private header. Behavior, public API and exported symbols do not change.

The fixes in #740 keep the first definition in composition order and give every
later one a name owned by its file or module, for example `der_traverse` in
`der_validation.c`, `lldp_schema_rules` in `lldp/schema.c` and
`backend_source_t` in `query/document_backend.c`. The Schema copies of the Query
diagnostic helpers are `schema_query_*`. `EMV_COUNT` is `#undef`-ed. In one
translation unit a private header is part of the main file, so Clang's
`-Wunused-function` also reports its unused `static inline` functions:
`query_input_location` and `query_relate_expression` in the Schema copy were
unused and are removed.

Not a blocker, recorded for a separate issue: 22 external symbols lie outside
the `tlv_` namespace. They are the 12 Query owner-internal symbols (`query_*`)
and 10 hidden internal symbols (`document_create_node`,
`document_memory_allocate`, `document_memory_release`, `document_node_location`,
`document_node_tag`, `document_parse_list`, `document_set_offset`,
`emv_base_fields`, `emv_value_decode`, `emv_value_encode`). Static archives
already expose them to consumers' links; the amalgamation does not change that.

## Notices

- Each inlined file keeps its SPDX and copyright header in place.
- Each generated file starts with an SPDX license identifier line for MIT and
  the distinct copyright lines of its inputs in first-seen order, followed by a
  notice that it is generated, from which Library version and commit, and
  that changes belong in the modular sources.
- Every current input is MIT-licensed Library code. No third-party code is
  composed, and the portable export block replaces CMake's export header text.
  An input with another SPDX license identifier fails generation until this
  contract and [THIRD_PARTY.md](../../THIRD_PARTY.md) cover it.
- The published archive includes `LICENSE` and `LICENSES/MIT.txt` (#744).
  The optional install component already includes both, `THIRD_PARTY.md`, and
  the SPDX/copyright notices retained in each generated file (#742).

## Determinism

The output bytes are a function of only these inputs:

- the content of the composed files, with line endings normalized to LF;
- the registry: target order, `SOURCES` and `PUBLIC_HEADERS` lists and
  dependencies;
- the version metadata and the Query features;
- this contract's version.

They do not depend on the source or build directory, the host operating
system, the CMake generator or version, single- or multi-configuration builds,
the build type, compiler, linkage or test options, time, locale or environment.
Outputs use LF line endings, end with a newline and contain no absolute path,
timestamp, user or host name. Generating on Windows and Linux from one commit
must produce identical bytes; checkouts with `core.autocrlf=true` are covered
by the LF normalization.

## Metadata

`opentlv-amalgamation.json` records:

| Field | Contents |
| --- | --- |
| `schema` | Amalgamation contract version, `1` |
| `library` | `OpenTLV` |
| `version`, `revision`, `commit`, `branch`, `tag` | The values in `version.h` |
| `configuration` | `query_frontend` and `query_set_operations` as booleans |
| `targets` | In composition order: `key`, `export_name`, `kind` and the composed `sources` and `public_headers` as source-root-relative paths |
| `inputs` | Every composed file in emission order with its relative path and the SHA-256 of its LF-normalized content |

The file lists no digest of the generated files; release checksums,
attestations and provenance belong to #744. It is generated deterministically,
with sorted object keys and LF line endings.

## Distribution equivalence gate

`scripts/check_distribution_equivalence.py` prepares one conventional Library
build, generates its amalgamation, installs and relocates its native binaries,
and runs the same external test project against all three distributions. It
requires matching version/revision/commit/branch/tag, Query configuration,
module composition, generated version/feature headers, compiler identity,
pointer size and build configuration. Relocated files must match the checksummed
installation manifest. Build and installed binary hashes are recorded separately
because installation can rewrite ELF RPATH. An older published release is not a like-for-like input;
release download verification and publication remain separate gates.

The test project reuses the existing unit and integration source lists,
including Core, IO, Document, Query, Schema, Codec, Generator and every builtin,
the C++ facade, Query conformance/adversarial cases, diagnostic contracts, and
the baseline observation probe. CTest names and GoogleTest scenario names and
multiplicities must match exactly, and every selected case must pass. Existing
white-box tests still use their private test headers; implementation objects
always come from the selected distribution. EMV source-isolation and binary ABI
snapshot checks remain in the conventional gates.

Ten Generator domains produce 320 deterministic cases per variant; their bytes
must match, and the shared property checker checks round trips. Every enabled
fuzz harness replays the checked-in valid/invalid seed corpus plus an empty
input, including Query program/image corruption. Query fixtures retain their
existing expected-result checks. Diagnostic strings, capability observations
and the baseline probe's results must compare exactly. Public API references
link every registry-declared C symbol through both header forms. Static builds
compare external C definitions and reject duplicates; opposite-order flat and
modular C++ header includes compile and link in separate translation units.
This is behavioral/source equivalence, not an amalgamation ABI promise.

Run locally with CMake 3.21+ (for CTest JUnit output), Ninja, Python 3.11, a supported C/C++ compiler, and
`nm` (`llvm-nm` on Windows) on PATH:

```sh
python scripts/check_distribution_equivalence.py \
  --output build/distribution-evidence --cc gcc --cxx g++ --shared OFF
```

Use a fresh output directory for each run. `--frontend OFF`,
`--set-operations OFF`, `--config Debug`, and `--sanitize` (Linux) select matching
configurations for every variant. `--gtest-source` can reuse a local checkout of
the repository's pinned GoogleTest version. Reports, complete command logs,
CTest/GoogleTest XML, corpus hashes and observations remain under the output
directory even on failure. `report.json` identifies the active failing variant.

Full and release CI require the `distributions` job. Its initial qualification
matrix includes GCC static/shared, MSVC static/shared, Clang ASan/UBSan Debug,
and all four Query feature combinations. Sanitizers instrument the conventional
libraries, amalgamation and test/corpus consumers. Fast/PR CI keeps the existing
consumer smoke checks without this suite. Missing, skipped or failed required
jobs fail the Full release gate, and failure artifacts retain variant names.

For epic [#701](https://github.com/OpenTLV/opentlv/issues/701), attach the
successful **Core Full CI** run URL, exact candidate commit and its
`distribution-*` artifacts after the branch is published. Local reports alone
do not qualify a hosted run or authorize publication; #743's hosted evidence
acceptance item remains pending until that run is available.

## Consumer contract

**C.** `opentlv.c` compiles as C99 (`-std=c99 -pedantic` or the equivalent)
with a [supported compiler](../reference/compilers.md), without CMake and
without a C++ compiler. A C99 consumer includes `opentlv.h`, or headers from the
`include/` tree, and compiles `opentlv.c` once. The public C API, error codes and
observable behavior equal the conventional build of the same commit and
configuration. #743 verifies this.

**C++.** The existing header-only C++ facade (C++11 minimum, verified with
C++17 consumers as #701 requires) works unchanged with the `include/` tree and
`opentlv.c` compiled as C. The facade needs no amalgamation-specific branch.
The [installed samples](../../tests/amalgamation/consumer/README.md) give exact
GCC, Clang and MSVC compile/link commands. The C driver compiles the implementation
once; the C++ driver links that object and supplies the C++ standard library.
No conventional OpenTLV library or import/export definition is required.

**Unsupported:**

- compiling `opentlv.c` as C++;
- linking the amalgamation and conventional OpenTLV libraries, or two
  amalgamations, into one program;
- redefining Query feature or Library version macros when compiling;
- treating a consumer-built shared library from the amalgamation as an ABI
  promise of the Library;
- selecting a subset of modules.

## Generation

A standalone Library build generates the amalgamation on request:

```sh
cmake -S . -B build
cmake --build build --target opentlv_amalgamation
```

The target runs `cmake -P cmake/amalgamation/generate.cmake` with two files that
configuration writes: `<build>/opentlv-registry.cmake`, the registry inventory
from `opentlv_finalize_modules()`, and `<build>/opentlv-amalgamation.cmake`,
which holds the source and build roots, the version metadata and the Query
features. `cmake/amalgamation.cmake` adds the target. The portable export block
is the template `cmake/amalgamation/export.h.in`. The script can also run
directly:

```sh
cmake -DOPENTLV_AMALGAMATION_CONFIG=build/opentlv-amalgamation.cmake \
      -DOPENTLV_AMALGAMATION_OUTPUT=<directory> -P cmake/amalgamation/generate.cmake
```

Every run regenerates. The script checks all inputs before it writes anything,
so a failed generation leaves earlier outputs in place. It replaces `include/`
on every run and writes LF line endings on every host. It needs no compiler.

| Check | Covers | CI tier |
| --- | --- | --- |
| CTest `amalgamation-generator` (labels `unit`, `amalgamation`) | Fixture registries: composition order, include resolution and deduplication, include guards, private-header boundaries, notices, metadata, compile definitions, generated inputs, UTF-8, and identical bytes from another location and from CRLF inputs | PR (Clang unit tests) |
| CTest `amalgamation-c99` (labels `integration`, `amalgamation`) | Generates this build's amalgamation twice and compares the bytes, then builds a consumer that compiles `opentlv.c` as C99 with `-pedantic-errors` and the Library's warning policy, and runs it | PR (Clang integration tests) |
| `scripts/check_cmake_integration.py` | The standalone build has the target, and neither the default build nor `install()` generates. Embedding builds have no such target | PR and Main (CMake consumers) |
| `scripts/check_amalgamation_consumers.py` | Explicit component installation, missing-output failures, licenses, metadata, unchanged public headers, relocation, direct C99/C++17 compiler consumers and the same samples linked to conventional static/shared libraries | PR: Clang 18, both linkage modes; Full: also GCC and MSVC; minimum CMake/custom data directory/disabled Query features on Clang |

Comparing the external symbols with the static module archives, and comparing
the full behavioral corpus with the conventional build, belong to #743.
The standalone comparisons here cover the samples' Core/IO, ASN.1 and LLDP
operations, including C++ Schema validation and Codec decoding.
The minimum-version check also covers private static-library dependencies in
the conventional package: CMake 3.16 uses explicit link properties to avoid
exporting its internal cross-directory lookup suffixes.

## Optional installation

From a standalone Library checkout, explicitly generate and then install:

```sh
cmake -S . -B build -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF -DOPENTLV_BUILD_CXX=OFF
cmake --build build --config Release --target opentlv_amalgamation
cmake --install build --config Release --component Amalgamation --prefix "$PWD/install"
```

The component contains the generated pair, `include/` tree and JSON metadata,
plus `LICENSE`, `LICENSES/MIT.txt`, `THIRD_PARTY.md` and the
[standalone samples and commands](../../tests/amalgamation/consumer/README.md)
under `examples/`. It installs to
`<prefix>/${CMAKE_INSTALL_DATADIR}/OpenTLV/amalgamation` (normally
`<prefix>/share/OpenTLV/amalgamation`). Relative GNUInstallDirs destinations
honor `--prefix` and `DESTDIR`; the installed directory can be copied elsewhere.
It contains no CMake package, compiled libraries or private build paths.

This is a source-only install component, available in standalone Library builds,
not a `find_package(OpenTLV COMPONENTS ...)` component or a new library target.
Neither the default build nor the default installation includes it, even after
generation. Installation only copies existing outputs: absent required files
fail with the build command, and no generation or compiler runs during install.
Regenerate explicitly after changing source files or Query options; installation
does not check whether previously generated outputs are stale. Generating and
installing the component needs no C++ compiler or conventional library build.

Run the consumer checks locally from an environment with CMake, Ninja, Python
and the selected C/C++ compilers (an MSVC Developer shell on Windows):

```sh
python scripts/check_amalgamation_consumers.py --cc gcc --cxx g++ --shared OFF
python scripts/check_amalgamation_consumers.py --cc gcc --cxx g++ --shared ON
```

CMake prepares the artifacts and the conventional comparison. The standalone
samples are compiled and linked directly with compiler commands, including a
C-only check with only the flat pair and the sample source available.

## Names

The names follow the [naming contract](naming.md#recorded-decisions): Library
identity, no legacy alias and no "core" in a multi-module artifact.

| Identity | Name | Status |
| --- | --- | --- |
| Build target | `opentlv_amalgamation` | Implemented in #740; the CMake identity checks require it |
| Output directory | `<build>/amalgamation/` | Implemented in #740 |
| Files | `opentlv.h`, `opentlv.c`, `include/`, `opentlv-amalgamation.json` | Implemented in #740 |
| Install component | `Amalgamation` (a CMake install component, not a package component) | Explicit opt-in installation under `${CMAKE_INSTALL_DATADIR}/OpenTLV/amalgamation` (#742) |
| Release asset | Candidate `OpenTLV-X.Y.Z-amalgamation.tar.gz`, top directory `OpenTLV-X.Y.Z-amalgamation/`, manifest record `kind` `amalgamation` | Held for #744; adding a published kind updates the [artifact contract](naming.md#artifact-contract-2) |
| Per-module artifacts | That module's export name, e.g. `opentlv-asn1.h` | Out of scope for version 1 |

## Decisions to approve

1. Version 1 composes all 19 compiled targets for one configuration; per-module
   and custom selection are out of scope.
2. The artifact set: `opentlv.h`, `opentlv.c`, the `include/` tree for C++ and
   module-path consumers, and `opentlv-amalgamation.json`.
3. The registry as the only composition input, read without JSON so that
   CMake 3.16 suffices, and the standalone-only explicit `opentlv_amalgamation`
   target.
4. Registration order as the composition order, with failure instead of
   reordering.
5. The portable export block with no new public identifier, and external
   symbols equal to those of the static archives.
6. Fixed Query features and version metadata as recorded inputs.
7. The source rules; the fixes for the listed collisions and the registration
   of generated public headers in #740; and the separate follow-up for the 22
   unprefixed external symbols.
8. The determinism inputs, LF normalization and the metadata fields.
9. The C99 and C++ consumer contract and its unsupported combinations.
10. The names above, with the install component and release asset held for
    #742 and #744.
