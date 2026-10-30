# Functional module contracts

This page defines the functional module architecture that epic
[#643](https://github.com/OpenTLV/opentlv/issues/643) implements. It is the
deliverable of [#644](https://github.com/OpenTLV/opentlv/issues/644) (Issue 1)
and is written for contributors who implement Issues 2 to 12 and for the
reviewers who approve these contracts. Read the
[architectural rules](../../concepts/architectural-rules.md) and the
[failure model](../../concepts/error-model.md) first.

**Implementation status:** #647 extracts the C `OpenTLV::Core` and
`OpenTLV::IO` targets, #648 extracts `OpenTLV::Document`, and #649 extracts
`OpenTLV::Generator`, using the
infrastructure from #646. Their sources, public
headers and symbols are registered in `tlv/modules.cmake`; the isolation gate
checks the compiled dependency graph. The remaining modules and C++ facades
retain the planned boundaries below and stay in the compatibility targets until
their extraction issues. See the [consumer migration guide](../../guides/select-components.md).

Document Query declarations now live in `tlv/query/document.h`; path matching
and batch-edit policy live in `query/document_path.c` and
`query/document_backend.c`. Both use `tlv/document/integration.h` and public node
APIs, with no Document private-header access. The integration remains compiled
in the compatibility library until #650 registers Query and its integrations.
The base Document target has no Query, Schema, Codec, Generator or builtin
headers, sources or symbols. C++ facade separation remains tracked below.

Generator registers `generator.c` and `tlv/generator.h`, with public Core and
private IO dependencies and its own export header. Its source and algorithm
version remain unchanged; IO never depends on Generator. The existing
`tlv++/generator.hpp` retains Generator ownership and is checked with an
independently linked C++11 consumer. It remains header-only and uses the current
shared C++ support headers pending their facade separation. Generator test
ownership includes the existing native/facade regression scenarios; builtin
Formats in those scenarios are test fixtures, not Generator dependencies.

The [pre-refactor behavioral baseline](../../../tests/baseline/README.md) for
issue #645 pins the released behavior and diagnostic inventory used when applying
these contracts. Approve that evidence before starting #646.

The public target names below (`OpenTLV::Core` and so on) are the working
names of the epic. Execution order puts
[#702](https://github.com/OpenTLV/opentlv/issues/702) first: its approved
naming contract decides the final CMake package, component, target and export
names. The contracts here use stable internal keys (`core`, `query-document`,
...) so a rename does not change any boundary.

| Deliverable | Where |
| --- | --- |
| Module ownership inventory | [ownership.json](ownership.json) `files`, summarized in [Ownership inventory](#ownership-inventory) |
| Public API ownership inventory | Header ownership plus the `api` moves in [ownership.json](ownership.json); see [Public API ownership](#public-api-ownership) |
| Dependency graph | [ownership.json](ownership.json) `targets`; see [Target dependency graph](#target-dependency-graph) |
| Cross-module integration ownership matrix | [Integration ownership](#integration-ownership) |
| Diagnostic architecture specification | [Module diagnostics](diagnostics.md) |
| Complete baseline diagnostic code mapping | [diagnostic-mapping.json](diagnostic-mapping.json); see [Code mapping](diagnostics.md#code-mapping) |
| Query feature decisions | [Query features](#query-features) |
| ASN.1 granularity decision | [ASN.1 granularity](#asn1-granularity) |
| Reader → Format → Field boundary specification | [Reader, Format and Field boundary](reader-format-field.md) |

`python scripts/check_module_contracts.py` verifies both JSON files against the
source tree without a native build; see [Enforcement](#enforcement).

## Modules and targets

A *module* is a unit of functionality with one owner. It has one public
*module target*, which consumers link and install as a CMake component. The
seven generic and six builtin modules are the public modules of the epic.

| Module | Key | Responsibility | Direct dependencies | Extracted by |
| --- | --- | --- | --- | --- |
| `OpenTLV::Core` | `core` | Definition, Format contract and composition, Layout, Element, Value, Tag, size and endian utilities, Field Encoding, result taxonomy, generic diagnostic infrastructure, version | none | [#647](https://github.com/OpenTLV/opentlv/issues/647) |
| `OpenTLV::IO` | `io` | Reader, Tree Reader, visitor traversal, Writer, Tree Writer, Element copy through a Format | Core | [#647](https://github.com/OpenTLV/opentlv/issues/647) |
| `OpenTLV::Document` | `document` | Owned mutable Document, wire import and export | Core, IO | [#648](https://github.com/OpenTLV/opentlv/issues/648) |
| `OpenTLV::Query` | `query` | Query text frontend, programs, plans, buffer and event execution | Core, IO | [#650](https://github.com/OpenTLV/opentlv/issues/650) |
| `OpenTLV::Schema` | `schema` | Schema definitions, length and value constraints, structural validation and reports | Core, IO | [#652](https://github.com/OpenTLV/opentlv/issues/652) |
| `OpenTLV::Codec` | `codec` | Value codec contract and generic numeric, digit, text, IPv4 and byte codecs | Core | [#651](https://github.com/OpenTLV/opentlv/issues/651) |
| `OpenTLV::Generator` | `generator` | Deterministic wire-data generation | Core; IO privately | [#649](https://github.com/OpenTLV/opentlv/issues/649) |
| `OpenTLV::ASN1` | `asn1` | BER, DER and CER formats, identifiers, validation, ASN.1 codecs, DER Schema, ASN.1 Query hooks | Core, IO, Codec, Schema, Query | [#653](https://github.com/OpenTLV/opentlv/issues/653) |
| `OpenTLV::EMV` | `emv` | EMV framing, DOL, dictionary, codecs, schemas, presentation, Query hooks | Core, Codec, Schema, Query; ASN.1 and Schema numbers privately; IO for the C++ facade | [#653](https://github.com/OpenTLV/opentlv/issues/653) |
| `OpenTLV::Bluetooth` | `bluetooth` | Bluetooth LTV format, AD data, definitions, schema and codecs | Core, IO, Codec, Schema | [#653](https://github.com/OpenTLV/opentlv/issues/653) |
| `OpenTLV::NFC` | `nfc` | NFC Type 2 framing | Core; IO for the C++ facade | [#653](https://github.com/OpenTLV/opentlv/issues/653) |
| `OpenTLV::DHCP` | `dhcp` | DHCPv4 option framing, definitions, options container and codecs | Core, Codec, Schema; IO privately | [#653](https://github.com/OpenTLV/opentlv/issues/653) |
| `OpenTLV::LLDP` | `lldp` | LLDP framing, definitions, LLDPDU validation and codecs | Core, Codec, Schema; IO privately | [#653](https://github.com/OpenTLV/opentlv/issues/653) |

An *integration target* owns only the public API that combines modules which
must not depend on each other. It has exactly one owning module, depends on
that module and on the other participants, and is never a dependency of the
participants themselves. Integration targets are built, installed and
exported like modules and are linked explicitly by the consumers that use
them, as [rule 18](../../concepts/architectural-rules.md#18-features-must-remain-independently-usable)
requires ("capabilities compose through explicit integrations"). They do not
count as public modules. A *facade-only* integration target is a header-only
`INTERFACE` target for C++ conveniences that have no C implementation.

| Integration target | Owner | Direct dependencies | Contents | Implemented by |
| --- | --- | --- | --- | --- |
| `query-document` | Query | Core, IO, Document, Query | Document Query visit, evaluation, values, programs, edits and `tlv_document_find_path()` | [#648](https://github.com/OpenTLV/opentlv/issues/648) prepares, [#650](https://github.com/OpenTLV/opentlv/issues/650) completes |
| `query-codec` | Query | Query, Codec | Codec-backed conversion adapters and builtin hooks | [#650](https://github.com/OpenTLV/opentlv/issues/650) |
| `codec-structure` | Codec | Core, Codec, Schema; IO privately | Structure Codec | [#651](https://github.com/OpenTLV/opentlv/issues/651) |
| `schema-number` | Schema | Schema, Codec | Numeric Schema constraints through number codecs | [#652](https://github.com/OpenTLV/opentlv/issues/652) |
| `schema-query` | Schema | Core, IO, Schema, Query | Contextual Query assertions over buffers and events | [#652](https://github.com/OpenTLV/opentlv/issues/652) |
| `schema-query-document` | Schema | Document, `schema-query`, `query-document` | `tlv_schema_query_validate_document()` | [#652](https://github.com/OpenTLV/opentlv/issues/652) |
| `document-codec` (facade only) | Document | Document, Codec | Typed decoding of Document nodes in C++ | [#648](https://github.com/OpenTLV/opentlv/issues/648) |
| `io-codec` (facade only) | Codec | IO, Codec | Typed `writer_base::write()` overloads in C++ | [#651](https://github.com/OpenTLV/opentlv/issues/651) |

The *aggregate* interface target links every public module and integration
target. It owns the umbrella headers `tlv/tlv.h`, `tlv++/tlv.hpp` and
`tlv++/native.hpp`, and nothing depends on it. It replaces "link everything"
use of the former monolithic `tlv` target; [#654](https://github.com/OpenTLV/opentlv/issues/654)
decides its final name with the #702 contract.

Builtins stay one module per protocol family. Their integrations with Codec,
Schema and Query stay inside the family module: builtins may depend on any
generic module or integration target, so no cycle can result, and a separate
target per builtin integration would only multiply public names. EMV depends
privately on ASN.1 for the BER identifier reader of its DOL helper; epic
[#695](https://github.com/OpenTLV/opentlv/issues/695) replaces that helper
with generic composition and then removes the dependency.

### C++ facades

Every module and integration target owns the header-only C++ facade of its C
API. The facade is a separate `INTERFACE` target per module that requires C++11
and depends on its C target and on the facades of that target's dependencies.
Pure C consumers never link a facade target, so they never need a C++
compiler, and `OPENTLV_BUILD_CXX` keeps controlling only C++ tests, examples
and tools. A facade may depend on more than its C target only where
[ownership.json](ownership.json) lists a `facade` dependency (EMV and NFC
facades read and write through the IO facade). A facade that combines two
modules belongs to their integration target.

## Target dependency graph

[ownership.json](ownership.json) `targets` is the authoritative graph. Each
target lists its direct dependencies: `public` ones are required by its public
headers, `private` ones only by its sources, and `facade` ones only by its C++
headers. The graph is acyclic, and the checker rejects any change that adds a
cycle. Generic modules:

```text
core <- io <- document
          <- query
          <- schema
          <- generator (private)
core <- codec
```

Integration targets sit above the modules they join:

```text
query + document          <- query-document
query + codec             <- query-codec
codec + schema (+ io)     <- codec-structure
schema + codec            <- schema-number
schema + query            <- schema-query
schema-query + query-document <- schema-query-document
```

Builtins depend on generic modules and integration targets, never the reverse:

```text
asn1      -> core, io, codec, schema, query
emv       -> core, codec, schema, query; asn1, schema-number (private)
bluetooth -> core, io, codec, schema
nfc       -> core
dhcp      -> core, codec, schema; io (private)
lldp      -> core, codec, schema; io (private)
```

Permitted directions:

1. Core depends on nothing. IO depends on Core.
2. Document, Query, Schema and Generator depend on IO; Codec depends only on
   Core. No generic module depends on another generic module at the same level.
3. An integration target depends on its owner and the other participants;
   no module target depends on an integration target of a generic module,
   except builtins.
4. Builtins depend on generic targets. A builtin may depend on another
   builtin only with a documented reason (EMV on ASN.1 above).
5. Nothing depends on the aggregate.

### Static and shared dependencies

The lists map directly to CMake: `public` dependencies are
`target_link_libraries(... PUBLIC ...)`, `private` ones `PRIVATE`. A shared
module records every dependency as a runtime dependency (`DT_NEEDED`, an
import library on Windows), while only public dependencies propagate include
directories and link options to consumers. A static module also propagates
private dependencies as link-only requirements (`$<LINK_ONLY:...>`), because a
static archive cannot carry its dependencies. The installed package exports
the same graph per component, so `find_package(OpenTLV COMPONENTS EMV)` makes
Core, Codec, Schema, Query, ASN.1, Schema numbers and IO available. Static and
shared builds keep the same graph; neither may need an extra dependency.

## Ownership inventory

[ownership.json](ownership.json) `files` assigns every C source, private
header, public header, C++ facade header and resource template under `tlv/`
and `tlv++/` (256 files) to one target. Generated headers (`tlv/config.h`,
`tlv/export.h`, `tlv/version.h`) belong to Core until #654 removes or moves
their module-selection parts. The inventory and symbol counts below come from
the current `main` revision; exported symbols are those of the shared library,
internal ones are global but hidden.

| Target | Sources | Private headers | Public C headers | C++ headers | Exported symbols | Internal symbols |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `core` | 20 | 5 | 23 | 12 | 109 | 5 |
| `io` | 5 | 1 | 8 | 9 | 64 | 1 |
| `document` | 3 | 1 | 1 | 2 | 34 | 9 |
| `query` | 5 | 4 | 3 | 5 | 46 | 12 |
| `schema` | 4 | 2 | 3 | 3 | 15 | 0 |
| `codec` | 6 | 1 | 7 | 5 | 23 | 0 |
| `generator` | 1 | 0 | 1 | 1 | 2 | 0 |
| `query-document` | 2 | 0 | 0 | 0 | 7 | 2 |
| `query-codec` | 1 | 0 | 1 | 0 | 2 | 0 |
| `codec-structure` | 1 | 0 | 1 | 1 | 2 | 0 |
| `schema-number` | 1 | 1 | 1 | 0 | 2 | 0 |
| `schema-query` | 1 | 0 | 1 | 0 | 2 | 0 |
| `schema-query-document` | 0 | 0 | 0 | 0 | 1 | 0 |
| `asn1` | 13 | 5 | 9 | 6 | 72 | 46 |
| `emv` | 8 | 2 | 7 | 4 | 188 | 3 |
| `bluetooth` | 9 | 0 | 9 | 3 | 18 | 0 |
| `nfc` | 1 | 0 | 1 | 1 | 1 | 0 |
| `dhcp` | 3 | 0 | 4 | 3 | 3 | 0 |
| `lldp` | 3 | 0 | 3 | 2 | 9 | 0 |
| aggregate | 0 | 0 | 1 | 2 | 0 | 0 |
| removed (`tlv_config_*`) | | | | | 15 | |
| **Total** | **87** | **22** | **84** | **59** | **615** | **78** |

The four resource templates (`tlv/resources/*.in`) belong to Core and are not
counted above. Symbol counts assign each symbol to its target after the `api`
moves below; Query and Document files that implement `query-document` stay in
their current directories until #650 moves them. The symbols were collected
from a GCC build of `main` with `nm` (static objects for definitions and
references, `nm -D` on the shared library for exports); #646 automates this
for every build.

### Public API ownership

A public declaration is owned by the target that owns its header. The `api`
section of [ownership.json](ownership.json) lists every declaration or
definition whose owner differed from its file in the #644 baseline, each with
the resolution that moves it. #647 completes the Core/IO moves and relocates
the other diagnostic name implementations to their owning directories:

| Moved API | From | To | Resolution |
| --- | --- | --- | --- |
| `tlv_copy_element()` | Core `copy.h` | IO | `core-copy-element` |
| `tlv_utf8_*` helpers | Core private header | Core public API | `core-utf8-api` |
| Reader and Writer operation names | Core `diagnostic_strings.c` | IO | `io-diagnostic-names` |
| `tlv_tree_writer_measure_events_observed()` | IO private helper without `tlv_` prefix | IO public API | `io-tree-measure` |
| `tlv_document_query_*`, `tlv_document_find_path()` and their types | Document `document.h` | `query-document` | `document-query-boundary` |
| `document_contains()`, `document_format()`, `document_node_tag()` | Document internals | Document public boundary | `document-query-boundary` |
| `query_document_*`, `query_live_overlap()`, `query_retained_overlap()` | Query internals | Query owner-internal interface | `query-document-integration` |
| `tlv_query_definition_resolve()` | `query-codec` `adapters.h` | Query | `query-codec-hooks` |
| Query kind and cause names | Core `diagnostic_strings.c` | Query | `query-diagnostic-names` |
| `tlv_codec_diagnostic_init()` and Codec names | Core `diagnostic.c`, `diagnostic_strings.c` | Codec | `codec-diagnostic-vocabulary` |
| Schema issue, definition and cause names | Core `diagnostic_strings.c` | Schema | `schema-diagnostic-causes` |
| `tlv_schema_query_validate_document()` | `schema-query` | `schema-query-document` | `schema-query-integration` |
| `tlv_config_query_frontend()`, `tlv_config_query_set_operations()` | Core `config.h` | Query | `query-features` |
| Other `tlv_config_*()` mirrors | Core `config.h` | removed | `remove-module-selection` |

The name functions currently promise availability "even when the corresponding
capability is disabled". In the module architecture a module is never
disabled: the name function is available wherever its module is linked, and a
consumer that holds a diagnostic of a module has linked it. Cause frames carry
their domain's name lookup, so a consumer can name a foreign cause without
linking its module; see [Module diagnostics](diagnostics.md#names).

## Existing cycles

The pre-extraction `tlv` target compiled everything together, so its include and
symbol graph contained cycles that separate targets cannot have. Assigning the
baseline files to their target owners gave these cycles, computed by
`python scripts/check_module_contracts.py --report` and by `nm` symbol
references:

The C Core cycles and Core/IO private-header exceptions are resolved by #647.
The C++ error facade still needs Schema, and the higher-module cycles remain
scheduled below. `ownership.json` records only the remaining exceptions.

| Cycle | Evidence | Resolution | Issue |
| --- | --- | --- | --- |
| Core ↔ IO | `copy.c` calls `tlv_write_element()` and `tlv_element_encoded_size()`; `writer.c` calls `tlv_copy_encoded()`; `diagnostic_strings.c` includes Reader and Writer headers | `core-copy-element`, `io-diagnostic-names` | #647 |
| Core ↔ Query | `diagnostic_strings.c` includes `tlv/query/program.h` | `query-diagnostic-names` | #650 |
| Core ↔ Codec | `diagnostic.c` defines `tlv_codec_diagnostic_init()`; both diagnostic files include `tlv/codec/diagnostic.h` | `codec-diagnostic-vocabulary` | #651 |
| Core ↔ Schema | `diagnostic_strings.c` and `tlv++/error.hpp` include `tlv/schema/schema.h` | `schema-diagnostic-causes` | #652 |
| Codec ↔ Schema | `tlv_codec_detail_t` embeds `tlv_schema_detail_t`; `tlv_schema_diagnostic_t` embeds `tlv_codec_detail_t` | `codec-diagnostic-vocabulary`, `schema-diagnostic-causes` | #651, #652 |
| Query ↔ `query-codec` | `tlv++/query/options.hpp` includes `tlv/query/adapters.h` for codec hooks | `query-codec-hooks` | #650 |
| Document ↔ Query (modules) | C boundary resolved in #648; Query-owned implementations use public Document APIs. Query target extraction and C++ facade separation remain | `document-query-boundary`, `query-document-integration` | #648, #650 |
| Query ↔ Schema (modules) | `tlv_query_diagnostic_t` embeds Schema and Codec detail; `schema/query.c` includes `query/program_internal.h` | `query-codec-hooks`, `schema-query-integration` | #650, #652 |

Every resolution is approved with this page. After them the target graph in
[ownership.json](ownership.json) has no cycle, which the checker verifies.

### Dependency and private-header exceptions

Besides the cycles, these includes reach a target outside the declared
dependencies or a private header of another module. The `exceptions` of
[ownership.json](ownership.json) list each include, and an extraction removes
its entries when it applies the resolution:

| Resolution | Includes it removes |
| --- | --- |
| `core-copy-element` | `copy.c` → `tlv/writer/writer.h` |
| `core-utf8-api` | `utf8_internal.h` from ASN.1 `asn1_values_internal.h`, Bluetooth `ad_codec.c` and Query `adapters.c`, `compiler.c`, `evaluation.c`, `execute.c` |
| `core-callback-api` | `callback_internal.h` from IO `writer/tree.c`, Codec `result_internal.h`, Query `program_internal.h` and ASN.1 `der_schema.c` |
| `io-diagnostic-names` | `diagnostic_strings.c` → Reader and Writer headers |
| `io-tree-measure` | `query/document_backend.c` → `writer/tree_internal.h` |
| `document-query-boundary` | `document.h` → `tlv/query/query.h`; `document.hpp` → Query facades |
| `document-codec-facade` | `document.hpp` → `tlv++/codec/typed.hpp` |
| `query-document-integration` | Document private-header access resolved in #648; Query owner-internal exports remain for #650 |
| `query-codec-hooks` | `tlv/query/program.h` → `tlv/codec/codec.h`; `query/evaluation.c` → `codec/result_internal.h`; `tlv++/query/options.hpp` → `tlv/query/adapters.h` |
| `query-diagnostic-names` | `diagnostic_strings.c` → `tlv/query/program.h` |
| `codec-diagnostic-vocabulary` | `diagnostic.c`, `diagnostic_strings.c` → `tlv/codec/diagnostic.h`; `tlv/codec/diagnostic.h` → Reader and Schema detail headers |
| `io-codec-facade` | `tlv++/codec/typed.hpp` → `tlv++/writer/writer.hpp` |
| `schema-diagnostic-causes` | `tlv/schema/schema.h` → `tlv/codec/diagnostic.h`; `diagnostic_strings.c` and `tlv++/error.hpp` → `tlv/schema/schema.h` |
| `schema-number-api` | EMV `emv_codec.c` → `schema/codec_internal.h` |
| `schema-query-integration` | `schema/query.c` → `query/program_internal.h`; `tlv/schema/query.h` → `tlv/document/document.h` |

`core-diagnostic-causes` (#647) has no include of its own: it adds the Core
cause infrastructure that `query-codec-hooks`, `codec-diagnostic-vocabulary`
and `schema-diagnostic-causes` require. The summaries in
[ownership.json](ownership.json) `resolutions` state each resolution in full.

### Cross-target internal symbols

A shared module exports only its declared API. These hidden symbols are
called across targets today and therefore need a resolution before the
targets are separate libraries:

| Symbols | Caller → owner | Resolution |
| --- | --- | --- |
| `tlv_utf8_validate()`, `tlv_utf8_find_invalid()`, `tlv_utf8_stream_*()` | ASN.1, Bluetooth, Query, `query-codec` → Core | `core-utf8-api` |
| Resolved in #648: public `tlv_document_callback()`, `tlv_document_callbacks_active()`, `tlv_document_contains()`, `tlv_document_options()` and `tlv_node_tag()`; batch-edit helpers are local to Query | `query-document` → Document | `document-query-boundary` |
| `tree_writer_measure_events_observed()` | `query-document` → IO | `io-tree-measure` |
| `query_document_end()`, `query_document_handle()`, `query_document_next()`, `query_document_result_count()`, `query_live_overlap()`, `query_retained_overlap()` | `query-document` → Query | `query-document-integration` |

All other cross-target references already use exported symbols. No exported
symbol lacks a public declaration.

## Private-header boundaries

- A header under `tlv/src/` is private to the module that owns it. Files of
  that module, including its integration targets, may include it; no other
  module may.
- An integration target may use its owner's private headers, but only the
  public API of the other participants. Document–Query is owned by Query, so
  it may use Query internals but needs a public Document boundary.
- An out-of-line function that an integration calls in its owner is an
  *owner-internal interface*: declared in the owner's private header,
  exported with the owner's export macro, absent from user documentation and
  without compatibility promise, and referenced only by the owner's
  integration targets (checked by #646). It is the only exported API that is
  not public.
- Public headers include only public headers of their target and its declared
  dependencies.
- Core's generic helpers that other modules need become documented Core API
  (`core-utf8-api`, `core-callback-api`) rather than shared private headers.

## Integration ownership

| Integration | Owner | Strategy | Target |
| --- | --- | --- | --- |
| Document–Query | Query | Query owns the integration; Document exposes a public traversal and edit boundary without depending on Query. #648 moves the Document Query declarations, types and edit helpers out of the Document target and keeps them building in place against that boundary; #650 moves the implementation into `query-document`, replaces private Document and Writer access, and makes the Query internals it uses an owner-internal interface. | `query-document` |
| Query–Codec | Query | Base Query keeps conversion hooks but reports their failures through Core cause frames; Codec-backed adapters, builtin hooks and C++ hook options move to the integration. Basic Query builds without Codec. | `query-codec` |
| Schema–Codec | Schema for numeric constraints, Codec for Structure Codec | Two directions, two targets: numeric constraints use number codecs (`schema-number`); Structure Codec composes Reader, Schema and value codecs (`codec-structure`). Neither base module includes the other; foreign detail travels as cause frames. | `schema-number`, `codec-structure` |
| Schema–Query | Schema | Contextual assertions use only public Query execution APIs. Document-based validation needs Document Query and is a separate target so buffer and event validation do not link Document. | `schema-query`, `schema-query-document` |
| Document–Codec (C++) | Document | Typed node decoding moves out of `document.hpp`. | `document-codec` |
| IO–Codec (C++) | Codec | Typed writer overloads move out of `typed.hpp`. | `io-codec` |
| Builtin–generic | Each builtin | Inside the family module, which depends on the generic targets it uses. | builtin module |

### Document–Query across #648 and #650

The repository stays buildable and testable between the two issues:

1. #648 creates the Document target with Core and IO only. Document Query
   declarations move to `tlv/query/document.h`. Path matching and selection-edit
   helpers move to Query-owned sources in the compatibility library. Generic
   callback lifetime protection stays with Document and is exposed through
   `tlv/document/integration.h`, alongside effective options and ownership checks.
   Query uses this boundary without accessing Document private headers.
2. #650 creates the Query target, completes `query-document` on public
   Document and IO APIs and the Query owner-internal interface, and removes
   the exceptions. The Document Query tests then run against the integration
   target.

## Static and shared builds

- Each compiled target has its own export macro and export header, generated
  per target; #646 defines the naming. A target compiles with hidden default
  visibility and exports only its own public API and its owner-internal
  interface.
- A target never re-exports symbols of another target. Header-only inline
  helpers are part of the API of the target that owns the header.
- Static targets keep their current object granularity, so a static consumer
  links only the archive members it references. Shared targets load each
  linked module completely.
- `OPENTLV_BUILD_SHARED_LIBS` keeps selecting static or shared libraries for
  every compiled target together. Pure C consumers link only C targets.

## Configuration options

The module architecture removes options that select functionality and keeps
build, language and development options. #654 removes options; extraction
issues stop using them for their own sources.

| Option | Decision |
| --- | --- |
| `OPENTLV_READER`, `OPENTLV_WRITER` | Remove; IO always provides both. |
| `OPENTLV_DOCUMENT`, `OPENTLV_QUERY`, `OPENTLV_SCHEMA`, `OPENTLV_CODEC` | Remove; module selection is linking. |
| `OPENTLV_FORMAT_ASN1`, `OPENTLV_FORMAT_BER`, `OPENTLV_FORMAT_DER`, `OPENTLV_FORMAT_CER` | Remove; see [ASN.1 granularity](#asn1-granularity). |
| `OPENTLV_EMV`, `OPENTLV_BLUETOOTH`, `OPENTLV_NFC`, `OPENTLV_DHCP`, `OPENTLV_LLDP` | Remove; module selection is linking. |
| `OPENTLV_QUERY_FRONTEND`, `OPENTLV_QUERY_SET_OPERATIONS` | Keep as Query-internal features; see [Query features](#query-features). |
| `OPENTLV_BUILD_SHARED_LIBS` | Keep. |
| `OPENTLV_BUILD_CXX` | Keep as the language switch for C++ tests, examples and tools. |
| `OPENTLV_BUILD_DOCS`, `OPENTLV_BUILD_EXAMPLES`, `OPENTLV_BUILD_TESTS` and the other `OPENTLV_BUILD_*` test, benchmark, fuzzing and Memcheck options, `OPENTLV_WARNINGS_AS_ERRORS` | Keep. |

The `tlv_config_*()` mirrors and `tlv/config.h` macros of removed options go
with them in #654. #647 removes Reader/Writer CMake options and source conditions,
but retains constant compatibility macros and runtime mirrors until then.
Remaining code that checks module-selection macros, such as
`document.c`, takes the always-available branch.

## Query features

Both Query features stay, as options of the Query target only:

| Feature | Decision and reason |
| --- | --- |
| `OPENTLV_QUERY_FRONTEND` | Keep. It omits the text parser and compiler (`compiler.c` and the V1 path parser in `query.c`) for frontend-free execution of [static C plans](../../guides/queries.md), a documented embedded use case. It adds no dependency on another module, so it is a feature of Query, not a module. |
| `OPENTLV_QUERY_SET_OPERATIONS` | Keep. It omits union, intersection and difference from both executors; compilation and plan validation reject them with `UNSUPPORTED` + `CAPABILITY`. The [Query guide](../../guides/queries.md) defines such operation-family switches as capability choices within one plan and execution architecture. |

Retained features must not change dependencies or what other modules see:

- Their macros and runtime mirrors move from Core `tlv/config.h` to a
  Query-owned configuration header (`query-features`, #650); no other module
  reads them.
- They no longer turn off unrelated tests, examples, benchmarks or fuzzers
  globally; the Query test targets select what they need (#646, #650).
- `query_plan_supported()` stops checking `OPENTLV_DOCUMENT`: Document-level
  plans are valid representations, executable through `query-document`.

## ASN.1 granularity

`OpenTLV::ASN1` is one public module containing BER, DER and CER, their
identifier and length helpers, strict universal value validation, ASN.1
codecs, DER Schema and the ASN.1 Query hooks. BER, DER and CER share
identifier, length and value-validation implementation
(`asn1_internal.c`, `ber_internal.c`, `asn1_values_internal.c`), so separate
public targets would need further internal targets without serving a
protocol family boundary.

This removes `OPENTLV_FORMAT_ASN1`, `OPENTLV_FORMAT_BER`,
`OPENTLV_FORMAT_DER` and `OPENTLV_FORMAT_CER`: a consumer can no longer build
BER without compiling DER and CER. Expected footprint:

- A static consumer links only the archive members it references. A BER-only
  program pulls `ber.c`, `ber_internal.c`, `identifier.c` and their Core
  dependencies, as it does today with DER and CER disabled; DER, CER, DER
  Schema and codec objects stay out unless referenced.
- A shared consumer loads the whole ASN.1 library and its dependencies (Core,
  IO, Codec, Schema and Query). Size-sensitive shared deployments should link
  ASN.1 statically.

For scale, a GCC 13 `MinSizeRel` static build of `main` has these ASN.1
object sizes (text + data + bss): BER format and helpers (`ber.c`,
`ber_internal.c`, `identifier.c`) 4.4 KiB, shared value validation 3.8 KiB,
DER/CER identifier helpers 0.9 KiB, DER format and validation 3.7 KiB, CER
format and validation 8.0 KiB, ASN.1 codecs 13.8 KiB, DER Schema 14.7 KiB and
Query hooks 1.2 KiB; 50.6 KiB in total. #653 records the actual before/after
footprint.

## Enforcement

`scripts/check_module_contracts.py` runs in the static-analysis pre-commit job
of every Core pull request, with its tests in
`scripts/test_module_contracts.py`. It checks:

- the seven generic and six builtin modules, valid targets and an acyclic
  dependency graph including facade dependencies;
- that every `.c`, `.h`, `.hpp` and `.in` file under `tlv/` and `tlv++/` has
  one owner, with no stale entries;
- that every quoted include stays within its target or a declared
  dependency, and never reaches another module's private header, except for
  the recorded exceptions; new and resolved-but-recorded exceptions both fail;
- that every `api` move and resolution is still current;
- that [diagnostic-mapping.json](diagnostic-mapping.json) maps every enumerator
  of the public diagnostic vocabularies and every Query limit name, with
  unique domains and no merged codes.

An extraction PR updates [ownership.json](ownership.json) with the code it
moves: new files get an owner, applied resolutions lose their exceptions and
`api` entries, and a resolution without remaining entries is removed.
The CMake and symbol gates below enforce the implemented build graph as modules
are registered during extraction.

### CMake infrastructure and naming

[#646](https://github.com/OpenTLV/opentlv/issues/646) supplies
[`cmake/modules.cmake`](../../../cmake/modules.cmake). It does not extract a
functional module or replace the existing `OpenTLV::tlv` and `OpenTLV::tlv++`
targets. The public naming decision remains with #702; explicit `EXPORT_NAME`
arguments keep that decision separate from internal ownership keys.

| Identity | Convention |
| --- | --- |
| Package | `OpenTLV`; `find_package(OpenTLV CONFIG REQUIRED COMPONENTS <name>)` |
| Internal key | Lowercase contract key, for example `query-document` |
| Build target | `opentlv_<key>` with hyphens replaced by underscores |
| Public target | `OpenTLV::<EXPORT_NAME>`; the component has the same name |
| Compiled export macro | `OPENTLV_<KEY>_API`, uppercase with underscores |
| Static usage requirement | `OPENTLV_<KEY>_STATIC_DEFINE`, propagated to consumers |
| Generated export header | `tlv/<key>/export.h` |
| Module test build target | `test-module-<key>` |
| CTest ownership label | `module:<key>` |

Register each compiled target with `opentlv_add_module(key ...)`, giving
`EXPORT_NAME`, `INCLUDE_DIR`, `SOURCES`, `PUBLIC_HEADERS`, `PRIVATE_HEADERS`,
`PUBLIC_DEPS`, `PRIVATE_DEPS`, `PUBLIC_SYMBOLS` and a runnable C `CONSUMER` source.
Paths are relative to the calling source directory. Public headers are installed
relative to `INCLUDE_DIR`; private headers are never installed. Lists of symbols
contain exact external names, including exported data. No symbol-prefix wildcard
grants ownership. Every registered source/header must agree with `ownership.json`.
Generated export headers are handled by the helper.
Feature-disabled C sources retain their contract owner but are omitted from that
configuration's `SOURCES` and symbol lists; headers retain their registered
boundaries. Source selection belongs to the registering module.

For an integration, set `OWNER` to its owning module and depend directly on that
module. `OWNER_SYMBOLS` declares the owner's exported internal interfaces, which
only integrations with the same owner may reference. Other hidden global
functions remain private even in static archives. Integrations are real
independently linkable targets, not aliases to their participants.

A `FACADE` registration has public headers and public dependencies, no compiled
sources, and a C++ `CONSUMER`. It propagates `cxx_std_11` through an `INTERFACE`
target. Register its own contract key and dependency edges in `ownership.json`;
assign its `.hpp` files to that key when moving them out of the original module.
Creating or installing facade targets does not enable a C++ compiler. C++ tests,
examples and benchmarks enable C++ when they actually compile C++ sources.

`opentlv_finalize_modules()` runs after registrations. It rejects cycles,
undeclared links, missing owners and sources added outside the registration. It
writes the per-configuration `modules-<config>.json` build inventory and the
installed component registry. Components load their dependencies transitively,
including private archive dependencies, before importing their own targets.
Unknown required components reject package discovery; unknown optional components
report `OpenTLV_<name>_FOUND=FALSE`. No-component discovery preserves existing
consumption and includes all available module targets. During the transition,
`tlv` and, when enabled, `tlv++` are the available legacy components; names of
unextracted modules are not advertised as working components.

All compiled modules follow `OPENTLV_BUILD_SHARED_LIBS`. Shared targets use hidden
default visibility, disable Windows automatic symbol export and export only
their public or owner-internal symbol lists through their own macros. Static
targets propagate their own static definition without suppressing another
module's exports. The ELF symbol gate compares shared exports exactly and checks
archive references against direct dependencies and symbol ownership.
Installed shared modules locate adjacent dependencies through `$ORIGIN` on ELF
or `@loader_path` on macOS, unless an installation RPATH was explicitly supplied.

### Isolation and test ownership during extraction

Run these checks for each extraction, alongside the existing contract checks:

```sh
python -B scripts/test_module_isolation.py
python -B scripts/test_module_isolation.py --integration
python -B scripts/check_module_isolation.py --shared OFF
python -B scripts/check_module_isolation.py --shared ON
python scripts/check_cmake_integration.py --shared OFF --cxx ON
python scripts/check_cmake_integration.py --shared ON --cxx OFF
```

The isolation runner requires an ELF platform with CMake, Ninja, C/C++ compilers
and GNU or LLVM `nm`. It configures a fresh directory for every compiled module,
builds only that target and its dependency closure, checks declared dependencies
against the architecture contract, checks quoted and angle-bracket includes,
rejects foreign private headers and unintended symbol dependencies, and builds
and runs one installed consumer per component from a relocated package. C
consumers enable only C; facade consumers link only their facade target and
inherit its C++ requirements. Platform runtime/toolchain symbols are external to
the OpenTLV module graph; unresolved `tlv_`/`opentlv_` symbols are rejected.

The real static/shared fixture exercises public and private dependencies,
owner-internal integration, hidden symbols, per-module exports and a C++ facade.
Negative tests deliberately introduce dependency cycles, duplicate ownership,
undeclared dependencies, private-header access and incorrect exports. The
`cmake-consumers.yml` job runs these gates for PR, Main and Full validation, and
automatically discovers new module registrations. Its existing Windows/Linux
consumer matrix continues to check the transitional library. ELF symbol evidence
does not claim Windows or macOS symbol inspection.

Before the first extraction, the production inventory contains zero extracted
modules. The fixture proves that the infrastructure works; it does not claim
that the monolith is already isolated. The #644 checker still governs all
unextracted code and its explicitly recorded migration exceptions.

Use `opentlv_add_module_test(name OWNER key SOURCES ... DEPENDENCIES ...)` for
extracted scenarios. A source belongs to one test owner; attempting to compile
the same scenario source into another module test fails configuration. The
helper creates a labeled CTest executable and adds it to `test-module-<key>`.
`cmake --build <build> --target test-module-<key>` builds that owner's tests;
`ctest --test-dir <build> -L '^module:<key>$' --output-on-failure` runs them.
On Windows, provide the linked shared modules on the test process's `PATH`.

Keep existing tests in their current targets until their behavior is extracted.
In that extraction PR, move the scenarios to the owning module's test target and
remove them from the old source list; preserve assertions and case coverage.
Tests combining modules belong to the integration that owns that behavior.
Use additional `DEPENDENCIES` only for fixtures needed by the scenario, not to
make a module test exercise unrelated behavior. Query feature switches select
Query tests locally when Query is extracted. Re-run the behavioral baseline and
the existing native suite to demonstrate that no scenarios disappeared.

## Decisions to approve

1. The seven generic and six builtin module boundaries, and the target graph
   in [ownership.json](ownership.json).
2. Integration targets with one owning module, explicitly linked by
   consumers, including the facade-only `document-codec` and `io-codec`.
3. Query owns Document–Query; Document exposes a public boundary.
4. The resolution of every cycle and exception listed above, including
   publishing the Core UTF-8 and callback helpers.
5. Owner-internal interfaces as the only non-public exported API.
6. Builtin integrations inside their family modules; EMV's private ASN.1
   dependency until #695.
7. Separate C++ facade targets per module, with `facade` dependencies.
8. The diagnostic architecture and code ownership mechanism in
   [Module diagnostics](diagnostics.md).
9. Keeping both Query features as Query-internal options.
10. One ASN.1 module with BER, DER and CER.
11. The Reader → Format → Field boundary in
    [reader-format-field.md](reader-format-field.md).
