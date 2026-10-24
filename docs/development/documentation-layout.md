# Where documentation belongs

First choose the owning repository using the
[documentation ownership rules and inventory](documentation-ownership.md).
Core contracts and C/C++ examples remain in Core; binding/tool documentation
follows its component, and portal infrastructure belongs to `opentlv-docs`.
The inventory records temporary co-location and the conditions for moving it.

Within an owner's documentation, each page lives in exactly one section. Choose
its section by the page's primary purpose, and link to related material elsewhere.
The paths below describe the current [docs/](../README.md) layout; they do not
assign consumer or portal content permanently to Core.

| Section | Put here | Example |
| --- | --- | --- |
| `getting-started/` | Introductory material: install, build, first program | [Getting started](../getting-started/README.md) |
| `concepts/` | Explanations of how and why the library works | [Architecture](../concepts/architecture.md) |
| `guides/` | Task-oriented how-tos for one job | [Memory ownership](../guides/memory.md) |
| `formats/` | Wire formats and their reading, writing and byte examples | [Formats](../formats/README.md) |
| `standards/` | Standards or industry semantics layered on a format | [DER](../standards/der/README.md) |
| `opentlv-cli/docs/` (external) | Command-line tool documentation | [otlv](https://github.com/OpenTLV/opentlv-cli/blob/main/docs/README.md) |
| `reference/` | Lookup material: API manuals, option tables, support matrices | [C API](../reference/c-api.md), [Compilers](../reference/compilers.md) |
| `development/` | Contributor material: tooling, fuzzing, docs conventions | [Fuzzing](fuzzing.md) |

## Progressive learning conventions

Name the intended audience and link prerequisite concepts near the beginning
of substantial learning pages. Introduce Element and Reader/Writer before
optional Query, Schema and Codec; keep contributor onboarding separate.

For tutorials and concepts, explain the problem, show a minimal useful example
with expected output, then explain normal usage. Place advanced ownership,
errors and contracts later, while keeping the safety rules needed for the first
example beside it. End with a recommended next step. Reference entries, audits
and release notes need not repeat this structure.

Use small fenced `text` diagrams with ASCII arrows and short lines that fit
narrow screens. Show one relationship at a time. Label descriptions, runtime
objects and operations accurately; optional consumers are not a mandatory
pipeline. Label future models explicitly and link to the roadmap.

The [Core index](../index.md) is the source entry point. The portal owns its
website homepage and [current presentation conventions](https://github.com/OpenTLV/opentlv-docs/blob/main/docs/development/documentation.md).
Component README files link to their language guide before implementation details. Link to canonical
support, memory, build, architecture and roadmap pages instead of copying
inventories. Keep established paths and anchors, or record migration links.

Use consistent names: Format defines wire rules, Element carries semantic Tag
and Value, Layout carries source ranges, Schema checks composition, and Codec
interprets Value. State allocation, streaming and preservation claims with their
actual scope. Verify marked examples, repository links and the strict site;
review rendered diagrams and first tasks at a narrow viewport.

## Section boundaries

The portal owns its seven-section navigation: Home, Tutorials, API, Standards,
Languages, Tools and Development. These sections are presentation destinations;
the source directories above retain their existing identities and ownership.
Shared API contracts and generic format configuration belong to API; protocol
material belongs to Standards. The portal
[conventions](https://github.com/OpenTLV/opentlv-docs/blob/main/docs/development/documentation.md)
and migration map record current output routes. Source paths need not move when
navigation changes.
The [architecture overview](../concepts/architecture.md) is the canonical model,
the [binding contract](../concepts/bindings.md) retains shared adaptation rules
and comparative evidence, and the [implemented inventory](../formats/support.md)
owns native availability. Current component capabilities belong to their owners. Task guides link to these inventories rather than redefine them.

`concepts/` explains a model or design decision; `guides/` shows how to complete
a task; `reference/` provides exact contracts, syntax, options and limits for
lookup. A guide should link to the relevant concept or reference instead of
copying it. Introductory build and first-program instructions belong in
`getting-started/`; contributor workflows belong in `development/`.

`formats/` describes wire encoding, identifier and length framing, format
callbacks and byte examples. `standards/` describes validation rules, semantics
and the limits of implemented support built on those formats. For example,
the DER and CER format pages describe their framing, while their validation pages
describe recursive and value validation. DER and CER are sibling ASN.1 encoding rules;
EMV composes framing, definitions, schemas and codecs for BER-TLV. Cross-link these pages rather than duplicating
their contracts or implying complete protocol support.

## Generated API reference

The C and C++ API references are generated with Doxygen and embedded in the
site under `api/reference/` at build time; they are never committed. The
hand-written pages [c-api.md](../reference/c-api.md) and
[cxx-api.md](../reference/cxx-api.md) are their entry points in the navigation.
Guides and concepts link to those pages (and to their per-area sections) rather
than into the generated files, which do not exist in the repository. Generated
pages link back to the guides through the `@docs` Doxygen alias. Generate the
references independently with Core CMake, or let the standalone portal generate
them during its site build; see
[CONTRIBUTING.md](../../CONTRIBUTING.md#generate-the-c-api-reference).

## Versioned documentation

The site is published per release: `latest` is the development documentation
built from the portal's pinned Core revision, and each new released `X.Y.Z` is
built from its selected release tag and stays available after later releases.
Historical `X.Y` versions remain on the archived Core site. Write pages for the current sources; do
not copy the documentation tree or add version-specific pages by hand. Links
that leave `docs/` point at the source of the same version. See
[CONTRIBUTING.md](../../CONTRIBUTING.md#publishing) for how versions are
published.

## Executable examples

Important examples (introductory C and C++ usage) are compiled sources under
`examples/`, not Markdown-only snippets. Place `<!-- example: examples/PATH -->`
directly above a fenced block to embed a source file verbatim; run
`python scripts/check_doc_examples.py --fix` to refresh the copy. CI fails when a
block differs from its source or the source no longer builds. Small illustrative
fragments may stay inline.

### Portal example slots

Core's root `docs-examples.json` associates stable feature IDs with native
executable sources. The portal reads these files from the selected Core revision;
Core builds and documentation checks do not require the portal. A feature ID
describes a specific operation and input contract, not an entire API subsystem.
`fixed.roundtrip` means the Fixed `Hello, world!` round trip described in the
[example guide](../guides/examples.md#fixed-round-trip-by-language).

A paired `<!-- opentlv-examples: fixed.roundtrip -->` and
`<!-- /opentlv-examples -->` slot wraps ordinary Markdown fallback links.
Keep that fallback useful when reading Core without the portal. Assembly replaces
the slot with source-backed language tabs without editing the original page.
Binding repositories own their own catalogs and examples; do not add their
sources to Core's native catalog. Existing `<!-- example: PATH -->`
blocks and their synchronization checks remain supported.

The portal documents the catalog format, unavailable states and verification
inputs in its [integration guide](https://github.com/OpenTLV/opentlv-docs#language-example-integration).
Declaring a source is not evidence of a successful test run.

## Language tabs

OpenTLV provides an idiomatic C++ API and official language bindings over the
canonical C execution engine (see the [language bindings conceptual
model](../concepts/bindings.md)). Where two or more expose the same operation,
show them side by side in content tabs instead of writing separate pages or
listing languages one after another:

````markdown
/// tab | C

C code and any C-specific notes.
///

/// tab | C++

C++ code and any C++-specific notes.
///

/// tab | Python

Python code and any Python-specific notes.
///
````

The `/// tab | Label` blocks (`pymdownx.blocks.tab`) keep their content
unindented, so the source remains valid Markdown that markdownlint accepts
without suppressions; do not use the indented `=== "Label"` syntax.

Conventions:

- Label tabs `C` and `C++` first, in that order, then one tab per binding that
  has an equivalent, in the order `Rust`, `Python`, `Lua`, `Go`.
  Tabs with the same label are
  linked across the site, so a reader's choice follows them from page to page.
  Use other labels only for further languages, never for other kinds of choice.
- Tab only what differs. Keep the explanation of behavior shared across
  languages (formats, lengths, ownership, errors) as ordinary text outside the
  tabs, and mention a language-specific difference in that language's tab.
- Use the same input bytes, Format configuration, Tag, Value and operation in
  every tab of a comparison. Quick starts write and read `Hello, world!` with
  Fixed (one tag byte, one big-endian length byte), tag `01`, without a trailing
  NUL. Show error handling through each language's public facade, and explain
  ownership or result-shape differences beside its example.
- Quick starts are showcases of each binding's current ergonomics. Use its most
  convenient public entry points for the chosen operation: direct Value inputs,
  tag helpers, scoped writing, iteration and typed decoding where supported.
  Revisit the examples when these APIs improve. Keep normal error handling and
  lifetime notes visible; put regression assertions and detailed native control
  in tests or advanced examples. Preserve each binding's supported baseline,
  including C++11.
- Use tabs only for equivalent functionality. If a binding has no counterpart
  for a group (for example Go has no public Schema facade, and
  the WebAssembly build is not a Reader/Writer-shaped binding at all — see
  [WebAssembly](https://github.com/OpenTLV/opentlv-js/blob/main/docs/webassembly.md)), leave it out of that group's tabs rather
  than adding an empty one; a shape difference that is still worth flagging
  (a narrower codec, a differently-shaped error type) belongs in prose or a
  cross-link to that binding's own guide, not a tab.
- Every tab group must read correctly on GitHub, which renders the source
  without tabs: put a short sentence before the group and give each tab enough
  context (a file name or a note) to stand alone.
- Important tabbed examples are compiled or CI-run sources: put the
  `<!-- example: PATH -->` marker and the fenced block inside the tab, and the
  check below keeps the copy identical. See
  [Getting started](../getting-started/README.md#quick-start). Small
  illustrative fragments (a guide traversing through one call, not a complete
  program) may instead stay inline per tab, with a "Runnable version" link to
  the full example file, as `guides/python.md` does.

## Adding new topics

- **OTLV**: the planned complete model language's syntax and specification belong
  in `reference/otlv/`, its model in `concepts/`, task-oriented usage in
  `guides/`, and tool commands in the owning tool repository. A definition language
  is distinct from the wire formats it describes; those remain in `formats/`.
- **Language bindings**: in the owning binding repository, one directory per
  binding under `reference/bindings/`,
  for example `reference/bindings/<language>/`, with task-oriented usage in
  `guides/`.
- **Additional tooling**: a page or directory in the owning tool repository;
  Core contributor tooling in `development/`.
- **New formats and standard-specific capabilities**: a directory under `formats/` or `standards/`
  with a `README.md`, as the existing ones do.

Create these pages and directories when content is available; this layout
does not require placeholder pages or new OTLV, binding or tool documentation.

## Rules

- Give a section or topic a `README.md` when it needs an overview; a directory
  used only to group pages does not need a placeholder index.
- Record every new page in the [ownership inventory](documentation-ownership.md#page-inventory).
  Add it to the `nav` in the [portal configuration](https://github.com/OpenTLV/opentlv-docs/blob/main/mkdocs.yml). Keep the
  GitHub index [docs/README.md](../README.md), site landing page
  [docs/index.md](../index.md), and project [README.md](../../README.md)
  aligned with the sections and entry points they describe. The GitHub index
  may link to an overview that leads to individual topic pages.
- When moving a page, update inbound links throughout the repository,
  including contributor documentation, examples and test documentation.
  Preserve heading anchors where possible and update fragment links when
  headings change.
- Run `python -m mkdocs build --strict` in the portal to check assembled site
  pages, anchors and source-file links. Review external links separately; the
  site build does not fetch external HTTP destinations.
- Preserve technical content and examples when reorganizing. Move detailed
  landing-page material to its canonical source, and use introductory bridges
  where prerequisite concepts are missing.

## Documentation inventory checks

These checks cover Core and the binding sources still hosted here. Their
[ownership boundaries](documentation-ownership.md#infrastructure-and-sources-outside-the-page-inventory)
separate native source validation from standalone component and portal checks. A Core documentation change does not require unrelated binding
updates, and inventory success is not evidence of binding feature support.

Run `python scripts/check_doc_inventory.py` and
`python scripts/test_doc_inventory.py` alongside the existing example check,
Markdown lint and generated API references. Core CI runs these checks without
MkDocs or the portal. The portal checks navigation against assembled Markdown
and runs the strict MkDocs build. Core retains binding checks while those
sources remain here. CLI checks belong to `opentlv-cli`; Lua and Python example
and test execution belong to `opentlv-lua` and `opentlv-python`.

The inventory check derives these small facts directly from repository sources:

| Source of truth | Checked documentation / configuration |
| --- | --- |
| Public config macros and root CMake options | Build-configuration rows in the architecture overview |
| Public built-in `tlv_format_t` descriptors | Implemented Format inventory |
| C++ built-in directories | C++ family links in the implemented inventory |
| Any Core-local binding directories | User guide exists; standalone bindings own their guides and portal navigation |
| Core-local C/C++ example sources | Marked source inventory in the examples guide; standalone JS/WASM, Go, Rust, Python and Lua sources are external links |
| Doxygen public roots, recursion and file patterns | Recursive discovery of C and C++ public headers |

Adding or removing a public capability must update the corresponding inventory,
user guidance and executable examples in the same change. Errors name the fact
and destination to update; removals also detect stale Format/example entries.
If source registration syntax changes, update the derivation and negative
fixtures together. The checker is deliberately a small inventory check, not a
C/C++ parser or automatic documentation generator.

The binding capability matrix, semantic constraints, allocation/lifetime claims,
protocol conformance and design rationale still require manual source/test
review. A passing inventory check does not prove complete parity, execute every
example, validate all prose or replace link/reference/snippet checks. Add new
machine-derived checks only when there is a clear source of truth; do not mark
an FFI declaration as a public capability.

## Migration for issue #186

This historical table records the source-layout migration at #186. Its site URL
examples describe that configuration, not the current portal routes. See the
[portal migration map](https://github.com/OpenTLV/opentlv-docs/blob/main/DOCUMENTATION-MIGRATION.md)
for the seven-section output layout.

The following paths are relative to `docs/`. Existing pages under `formats/`
and `standards/` retain their paths unless listed here.

| Previous path | Current path |
| --- | --- |
| `getting-started.md` | [getting-started/README.md](../getting-started/README.md) |
| `architecture.md` | [concepts/architecture.md](../concepts/architecture.md) |
| `core-types.md` | [concepts/core-types.md](../concepts/core-types.md) |
| `value.md` | [concepts/value.md](../concepts/value.md) |
| `length.md` | [concepts/length.md](../concepts/length.md) |
| `endian.md` | [concepts/endian.md](../concepts/endian.md) |
| `memory.md` | [guides/memory.md](../guides/memory.md) |
| `schemas.md` | [guides/schemas.md](../guides/schemas.md) |
| `codecs.md` | [guides/codecs.md](../guides/codecs.md) |
| `copy.md` | [guides/copy.md](../guides/copy.md) |
| `reader.md` | [guides/reader.md](../guides/reader.md) |
| `format-examples.md` | [formats/format-examples.md](../formats/format-examples.md) |
| `format-roadmap.md` | [formats/format-roadmap.md](../formats/format-roadmap.md) |
| `cli.md` | [cli/README.md](https://github.com/OpenTLV/opentlv-cli/blob/main/docs/README.md) |
| `compilers.md` | [reference/compilers.md](../reference/compilers.md) |
| `fuzzing.md` | [development/fuzzing.md](fuzzing.md) |

This migration updates repository links but does not add redirects or retain
stub pages at old source paths. External links and bookmarks to moved source
pages must be updated using the table above. Published site URLs also change
when a page's output path changes; for example, `architecture/` becomes
`concepts/architecture/`. Moves from `cli.md` to `cli/README.md` and from
`getting-started.md` to `getting-started/README.md` keep their existing site
URLs with the current MkDocs directory-URL configuration. Old links to
unchanged site URLs and preserved heading anchors continue to work.
