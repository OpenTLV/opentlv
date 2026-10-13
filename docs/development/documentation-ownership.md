# Documentation ownership and source boundaries

This guide is for contributors changing documentation or extracting a component
from the Core repository. Read the [architecture rules](../concepts/architectural-rules.md)
and [documentation layout](documentation-layout.md) first.

It records the source boundaries for [#605](https://github.com/OpenTLV/opentlv/issues/605)
under [the documentation extraction epic](https://github.com/OpenTLV/opentlv/issues/603).
Ownership below is the target responsibility; all inventoried paths currently
live in `OpenTLV/opentlv`. No page, example or publishing job moves in this step.
The standalone portal is not yet the replacement for the existing pipeline.

## Ownership rules

| Owner | Canonical responsibility |
| --- | --- |
| Core (`OpenTLV/opentlv`) | C and C++ APIs, shared semantics, architecture, Reader, Writer, Document, Query, Schema, Codec, built-in formats and standards, native examples, source comments, Core build and development instructions |
| Binding | Its public API, installation, ownership and lifetimes, idiomatic usage, supported features, dependency declarations, tests and executable examples |
| Tool | Its commands, options, input/output conventions, installation, tests and executable examples |
| Portal (`OpenTLV/opentlv-docs`) | Global navigation, presentation, shared assets, MkDocs configuration, assembly, version selection, integration of generated references and examples, site validation and publication |

C++ remains part of Core. Rust, Python and Lua documentation will move with
their implementations to `OpenTLV/opentlv-rust`, `OpenTLV/opentlv-python` and
`OpenTLV/opentlv-lua`; CLI documentation will move to `OpenTLV/opentlv-cli`.
Go and WebAssembly follow the same component ownership rule; their destination
repository names must be recorded when their extraction is scheduled. These
destinations are ownership assignments, not claims that a migration is complete.

Each fact has one canonical source. A portal build may assemble content from an
owning repository into disposable build output; it must not maintain an edited
copy of that content. Generated C/C++ references come from the selected Core
revision. Binding compatibility, feature coverage and tested examples are
separate facts supplied by the component, not inferred from Core support.

Core changes update Core documentation in the same change. They do not require
unrelated binding changes, a checkout of the portal, or successful portal
publication. Binding and tool releases remain independent of Core releases.

## Transitional exceptions

The inventory uses these dispositions. They describe follow-up work, not
additional repositories or new documentation formats.

| Code | Current location and responsibility | Exit condition |
| --- | --- | --- |
| Keep | Core owns and maintains this source in Core | No extraction is required; the portal consumes the source |
| Component | Binding/tool owners maintain their documentation here while their implementation remains here | Move source documentation and examples with that component, update inbound links and checks, then remove the old canonical content |
| Portal | Portal-owned content or infrastructure is temporarily hosted in Core | Move it after the independent pipeline preserves its behavior and passes validation; update links and remove the obsolete Core copy/job |
| Mixed | Core retains its contracts; embedded component content or portal instructions have temporary co-location | Separate the identified parts when their owner migrates, preserve anchors/links, and retain the Core source without copying its contracts into the portal |

For a mixed page, ownership applies to sections and examples as well as files:

- Core owns the C execution contract, C++ facade, native build options and C/C++
  examples. Binding owners own language-specific snippets and their source tests;
  tool owners own command examples and output contracts.
- The binding matrix in `concepts/bindings.md` currently combines Core adaptation
  rules with component feature evidence. Core retains the rules. Components own
  their feature evidence; the portal will present the cross-component matrix.
- `formats/support.md` retains native C/C++ support in Core. CLI identifiers and
  language preset availability follow their components; their combined
  presentation belongs to the portal.
- `guides/examples.md` retains native example coverage in Core. Binding example
  inventories follow their components; the combined index belongs to the portal.
- Core guides and Getting Started retain shared explanations and C/C++ usage.
  Existing binding tabs stay synchronized with the current binding sources until
  component extraction. Later portal assembly integrates tested examples from
  their owners; implementing that mechanism is outside #605.
- `development/workflow.md` and the Query verification pages retain Core checks
  and evidence. Binding/CLI checks follow their owners; future ecosystem
  compatibility orchestration must not become a Core release prerequisite.
- Documentation layout and audit pages retain Core contribution rules and the
  historical audit record. Site presentation, global navigation, mike deployment
  instructions and cross-component conventions follow the portal.

Temporary co-location does not transfer ownership. Existing monorepository
checks still run during the transition. They must be separated by responsibility
as the affected components move; do not silently skip missing sources or retain
mandatory consumer checkouts in Core CI.

## Page inventory

Paths are relative to `docs/`. Every Markdown page, including this guide, has a
row. Update the table when adding, moving or removing a page. The existing
[documentation audit](documentation-audit.md) records audience and learning
structure; this table records ownership rather than replacing that audit.

| Source page | Owner | Disposition / boundary |
| --- | --- | --- |
| [README.md](../README.md) | Core | Keep: repository documentation entry point |
| [index.md](../index.md) | Portal | Portal: unified user landing page; retain a Core entry point when moved |
| [assets/README.md](../assets/README.md) | Core / Portal | Retained Core README logo; shared site variants are maintained in the portal |
| [cli/README.md](../cli/README.md) | CLI | Component |
| [cli/json-schema.md](../cli/json-schema.md) | CLI | Component: CLI JSON representation |
| [concepts/architecture.md](../concepts/architecture.md) | Core | Mixed: repository map includes consumers |
| [concepts/architectural-rules.md](../concepts/architectural-rules.md) | Core | Keep |
| [concepts/bindings.md](../concepts/bindings.md) | Core / bindings / Portal | Mixed: adaptation contracts, feature evidence and combined matrix |
| [concepts/core-types.md](../concepts/core-types.md) | Core | Keep |
| [concepts/cxx-builtins.md](../concepts/cxx-builtins.md) | Core | Keep |
| [concepts/cxx-formats.md](../concepts/cxx-formats.md) | Core | Keep |
| [concepts/cxx-native-boundary.md](../concepts/cxx-native-boundary.md) | Core | Keep |
| [concepts/definition-boundaries.md](../concepts/definition-boundaries.md) | Core | Keep |
| [concepts/endian.md](../concepts/endian.md) | Core | Keep |
| [concepts/error-model.md](../concepts/error-model.md) | Core | Mixed: canonical error contract with binding mappings |
| [concepts/field-policies.md](../concepts/field-policies.md) | Core | Keep |
| [concepts/format-capabilities.md](../concepts/format-capabilities.md) | Core | Keep |
| [concepts/format-contract.md](../concepts/format-contract.md) | Core | Keep |
| [concepts/learning-model.md](../concepts/learning-model.md) | Core | Keep |
| [concepts/length.md](../concepts/length.md) | Core | Keep |
| [concepts/processing-pipeline.md](../concepts/processing-pipeline.md) | Core | Keep |
| [concepts/query-language.md](../concepts/query-language.md) | Core | Mixed: canonical Query contract with facade coverage |
| [concepts/runtime-model.md](../concepts/runtime-model.md) | Core | Keep |
| [concepts/value.md](../concepts/value.md) | Core | Keep |
| [development/abi-compatibility.md](abi-compatibility.md) | Core | Keep |
| [development/cxx-facade-performance.md](cxx-facade-performance.md) | Core | Keep |
| [development/documentation-audit.md](documentation-audit.md) | Core / Portal | Mixed: historical source audit and presentation decisions |
| [development/documentation-layout.md](documentation-layout.md) | Core / Portal | Mixed: source conventions and portal rendering/publishing rules |
| [development/documentation-ownership.md](documentation-ownership.md) | Core | Keep: source boundaries and transition inventory |
| [development/fuzzing.md](fuzzing.md) | Core | Mixed: native fuzzing and CLI/consumer harnesses |
| [development/go.md](go.md) | Go | Component |
| [development/indefinite-boundary-reuse.md](indefinite-boundary-reuse.md) | Core | Keep |
| [development/lua.md](lua.md) | Lua | Component |
| [development/python.md](python.md) | Python | Component |
| [development/query-adversarial.md](query-adversarial.md) | Core | Mixed: native verification and facade evidence |
| [development/query-phase-audit.md](query-phase-audit.md) | Core | Mixed: native implementation audit and facade evidence |
| [development/query-release.md](query-release.md) | Core | Mixed: native release checks and consumer compatibility evidence |
| [development/reader-decode-performance.md](reader-decode-performance.md) | Core | Keep |
| [development/rust.md](rust.md) | Rust | Component |
| [development/valgrind.md](valgrind.md) | Core | Mixed: native checks and CLI/consumer harness selection |
| [development/webassembly.md](webassembly.md) | WebAssembly / Portal | Component: embedding/build API; portal owns playground integration |
| [development/workflow.md](workflow.md) | Core / components / Portal | Mixed: checks and release/publication responsibilities |
| [formats/README.md](../formats/README.md) | Core | Keep |
| [formats/asn1/ber.md](../formats/asn1/ber.md) | Core | Keep |
| [formats/asn1/cer.md](../formats/asn1/cer.md) | Core | Keep |
| [formats/asn1/der.md](../formats/asn1/der.md) | Core | Keep |
| [formats/bluetooth/README.md](../formats/bluetooth/README.md) | Core | Keep |
| [formats/custom/README.md](../formats/custom/README.md) | Core | Keep |
| [formats/dhcp/README.md](../formats/dhcp/README.md) | Core | Keep |
| [formats/fixed/configurable.md](../formats/fixed/configurable.md) | Core | Keep |
| [formats/format-catalogue.md](../formats/format-catalogue.md) | Core | Keep |
| [formats/format-examples.md](../formats/format-examples.md) | Core | Keep |
| [formats/format-roadmap.md](../formats/format-roadmap.md) | Core | Keep |
| [formats/lldp-review.md](../formats/lldp-review.md) | Core | Keep |
| [formats/lldp/README.md](../formats/lldp/README.md) | Core | Keep |
| [formats/lldp/conformance.md](../formats/lldp/conformance.md) | Core | Keep |
| [formats/nfc/README.md](../formats/nfc/README.md) | Core | Keep |
| [formats/packed.md](../formats/packed.md) | Core | Keep |
| [formats/support.md](../formats/support.md) | Core / components / Portal | Mixed: native support, CLI names and binding presets |
| [formats/variable.md](../formats/variable.md) | Core | Keep |
| [getting-started/README.md](../getting-started/README.md) | Core / bindings | Mixed: native setup and language quick starts |
| [guides/codecs.md](../guides/codecs.md) | Core / bindings | Mixed: shared semantics and language examples |
| [guides/copy.md](../guides/copy.md) | Core | Keep |
| [guides/cxx-examples.md](../guides/cxx-examples.md) | Core | Keep |
| [guides/diagnostics.md](../guides/diagnostics.md) | Core / bindings | Mixed: shared error semantics and facade examples |
| [guides/document.md](../guides/document.md) | Core / bindings | Mixed: shared semantics and language examples |
| [guides/examples.md](../guides/examples.md) | Core / components / Portal | Mixed: source inventories and combined index |
| [guides/go.md](../guides/go.md) | Go | Component |
| [guides/lua.md](../guides/lua.md) | Lua | Component |
| [guides/memory.md](../guides/memory.md) | Core / bindings | Mixed: native ownership and facade lifetime rules |
| [guides/processing.md](../guides/processing.md) | Core | Keep |
| [guides/python.md](../guides/python.md) | Python | Component |
| [guides/queries.md](../guides/queries.md) | Core / bindings | Mixed: shared Query semantics and language examples |
| [guides/reader.md](../guides/reader.md) | Core / bindings | Mixed: shared Reader semantics and language examples |
| [guides/rust.md](../guides/rust.md) | Rust | Component |
| [guides/schemas.md](../guides/schemas.md) | Core / bindings | Mixed: native validation and facade examples |
| [guides/select-components.md](../guides/select-components.md) | Core | Mixed: Core configuration and consumer build recipes |
| [guides/writer.md](../guides/writer.md) | Core / bindings | Mixed: shared Writer semantics and language examples |
| [playground/index.md](../playground/index.md) | Portal | Portal: browser playground entry point |
| [reference/README.md](../reference/README.md) | Core / Portal | Mixed: Core reference index and site integration instructions |
| [reference/c-api.md](../reference/c-api.md) | Core | Keep: hand-written C API entry point |
| [reference/compilers.md](../reference/compilers.md) | Core | Keep |
| [reference/cxx-api.md](../reference/cxx-api.md) | Core | Keep: hand-written C++ API entry point |
| [reference/errors.md](../reference/errors.md) | Core / bindings | Mixed: canonical C codes and language mappings |
| [standards/cer/README.md](../standards/cer/README.md) | Core | Keep |
| [standards/der/README.md](../standards/der/README.md) | Core | Keep |
| [standards/emv/README.md](../standards/emv/README.md) | Core | Keep |

## Infrastructure and sources outside the page inventory

Paths in this table are relative to the repository root. A directory name alone
does not determine ownership: in particular, `tools/docs/` contains both native
API generation inputs and portal code.

| Source / artifact | Owner and disposition |
| --- | --- |
| `docs/assets/opentlv.svg` and `docs/assets/README.md` | Retain the Core README logo and attribution; shared visual design, favicon, header mark and variants are maintained in the portal |
| `docs/playground/playground.js`, `docs/playground/playground.css` in `opentlv-docs` | Portal: browser UI and styling, transferred by portal #6; Core retains a Markdown entry-point link |
| `mkdocs.yml`, `requirements-docs.txt`, `tools/docs/hooks.py`, `tools/docs/overrides/main.html` in the portal repository | Portal: configuration, dependencies, source-link rewriting, generated artifact integration and version banner; removed from Core by #608 |
| `.github/workflows/docs-validation.yml` | Core: read-only Markdown, example/inventory and Doxygen validation with API artifacts; replaces the removed `docs.yml` publisher |
| `tools/docs/Doxyfile.in`, `Doxyfile.cxx.in`, `Doxyfile.common.in`, `c-api.dox`, `cxx-api.dox` (all under `tools/docs/`) | Core: independent C/C++ extraction configuration, groups and landing pages; preserve C/C++ cross-links and `@docs` links when portal URLs change |
| `CMakeLists.txt`, `cmake/doxygen.cmake` | Core: retain `OPENTLV_BUILD_DOCS`, `c-api-docs` and `cxx-api-docs` generation without a portal checkout; normal builds do not require Doxygen |
| `tlv/include/`, `tlv++/include/` | Core: canonical public API declarations and Doxygen comments |
| `examples/tlv/`, `examples/tlv++/`, their README files and tests | Core: executable native examples and their verification |
| `bindings/*/` documentation, examples and tests | Respective binding: Component exception; shared native helpers follow their actual implementation owner |
| `tools/cli/` documentation, acceptance examples and tests | CLI: Component exception |
| `scripts/check_doc_examples.py` | Mixed: owners check snippets against their own tested sources; portal integration checks assembled examples against the selected component revisions |
| `scripts/check_doc_inventory.py`, `scripts/test_doc_inventory.py` | Native capability/Doxygen and existing component checks stay with Core; CLI/binding inventories follow components when extracted; assembled navigation is checked only in Portal |
| `.pre-commit-config.yaml`, `.markdownlint.json`, source/license checks | Each repository retains checks for its own maintained sources; portal extraction does not remove Core Markdown linting |
| `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `ROADMAP.md`, `AGENTS.md`, repository policies | Core: retain repository entry points, contribution rules, history and contracts; extract only portal/consumer-specific instructions where present |
| `tests/`, `benchmarks/` and native component README files | Implementation owner: Core for native tests/benchmarks; consumer-specific harnesses move with that consumer |
| Generated Doxygen HTML/tag files, site output and WebAssembly distribution files | Build artifacts, not additional canonical documentation sources; owning producers build them and Portal integrates selected outputs |

## Links crossing future repository boundaries

These links are mostly repository-relative today. They become cross-repository
links as owners migrate; preserve the relationship and fragment, not an assumed
single-repository base URL.

| Current source / link family | Target and migration responsibility |
| --- | --- |
| Core guides and `getting-started/README.md`: language tabs, `<!-- example: PATH -->` markers and runnable-example links | `bindings/*/examples/` and language guides: binding owns source/tests; Portal integrates selected examples without requiring binding updates for a Core-only documentation change |
| `concepts/bindings.md`, `formats/support.md`, `guides/examples.md` | Binding sources/tests, CLI registrations and examples: retain Core facts locally; move component facts with their sources and preserve portal navigation |
| `reference/c-api.md`, `reference/cxx-api.md`, format guides and native examples | `tlv/include/`, `tlv++/include/`, `examples/`: keep Core-relative source links in canonical Markdown; assembled links must target the selected Core revision |
| `docs/README.md`, root/component README files and `CONTRIBUTING.md` | Core pages and the unified landing page: preserve a Core entry point, update portal destinations and keep existing anchors where practical |
| Doxygen `@docs` links, `OPENTLV_DOCS_URL` in `cmake/doxygen.cmake`, C++ tag-file references and reference entry pages | Hand-written guides and sibling C/C++ HTML trees: preserve both directions when generation and publication are separated |
| `playground/index.md`, its JavaScript and `development/webassembly.md` | WebAssembly distribution/API and browser assets: component produces the module; Portal owns its selected-version integration |
| Portal `mkdocs.yml`, source hooks and theme links | The portal selects the owning repository and revision for each source; Core has no site configuration |
| `development/workflow.md`, documentation layout, `CONTRIBUTING.md` and `.github/workflows/docs-validation.yml` | Core validation and API artifacts remain independent; portal build/publication commands live in the portal README |

Keep published page paths and heading anchors where practical. Before moving a
source, find inbound links in docs, headers, component READMEs and contributor
files. Record changed paths and redirects where needed; do not remove content
because the current portal does not yet consume it. A successful site build does
not prove remote links or their release-specific fragments exist.

## Source contract for the standalone portal

Core supplies its maintained Markdown, relative assets, native example sources,
public headers and independent API generation targets from a selected Git tag or
commit SHA. The portal must record the resolved revision and consume those
sources without modifying the Core checkout or requiring optional consumers for
Core-only documentation. Local multi-root development may use the sibling Core
checkout, but must distinguish local changes from a reproducible release build.

Preserve relative page paths and anchors in assembled Core content. Resolve
source and edit links against that page's owning repository and selected
revision, including links outside `docs/`. Keep generated C and C++ references
from the same Core revision and preserve their sibling layout and cross-links.
Use the existing MkDocs Material rendering conventions for current pages.

The current tree still contains the explicit Component, Portal and Mixed
exceptions above; it is not a finished Core-only export. Their inventory tells
the extraction work which sources to retain, separate or move. This issue does
not define an assembly engine, component version manifest, compatibility service
or new publication workflow. Those belong to subsequent epic work. Remove the
old portal infrastructure only after its replacement has been validated.

## Cutover for issue #608

The cleanup is prepared across Core and
[`OpenTLV/opentlv-docs`](https://github.com/OpenTLV/opentlv-docs). Do not merge the
Core cleanup until the replacement has successfully built and deployed to
<https://opentlv.github.io/opentlv-docs/>. A successful validation job or local
preview alone is insufficient. Record the successful hosted run and its live
`latest/source-revision.json` in the cleanup PR; check the selected Core SHA,
guide pages and both generated API references.

Apply the changes in this order:

1. Enable GitHub Pages with **Source: GitHub Actions** in the portal repository
   and allow `main` in the `github-pages` environment. Complete a real deployment
   and verify the published content and its API/guide links.
2. Merge the portal navigation change first. It checks assembled Markdown rather
   than Core's removed `mkdocs.yml`, and works with the existing pinned Core
   revision. When publishing release documentation, validate its selection using
   an actual Core release tag; do not retarget historical release links to development.
3. Preserve the Core `gh-pages` branch and its branch-based Pages configuration
   as the historical archive. Disable the old **Documentation** (`docs.yml`)
   workflow in Core's Actions settings and cancel any queued or running publisher
   before merging cleanup. Do not rerun or re-enable that workflow from old tags.
   Update any repository rules that explicitly refer to its old check name.
4. Merge Core cleanup after the publication and content checks pass. Its new
   **Documentation validation** workflow only reads repository content and
   generates API artifacts; it has no deployment job or write permission.
   Core builds and releases neither check out the portal nor wait for it.
5. Update the portal's `core-source.json` to the full merged Core commit SHA and
   publish again. Do not pin an uncommitted preview or invent a future SHA. Check
   the live source metadata and both API trees after this final publication.

The repositories have separate Pages deployments and repository-scoped tokens.
The retained Core validator cannot overwrite the new deployment. Changing
repository settings and publishing are deployment operations; a local cleanup
branch does not perform them.

### Existing URLs and content

| URL family | Preservation policy |
| --- | --- |
| `https://opentlv.github.io/opentlv/` | Retain the existing site and root default as an archive |
| Core site `0.6/`, `0.7/`, `stable/`, `latest/` and their page fragments | Preserve existing `gh-pages` contents unchanged, including working playground assets; historical `latest` stops updating |
| `https://opentlv.github.io/opentlv-docs/` | New maintained portal; preserve guide paths and anchors under `latest/`, `stable/` and new full `X.Y.Z` releases |
| `https://marekcingel.github.io/OpenTLV/` | The former account URL returned 404 during cutover preparation; redirects require control of that account's Pages site and cannot be installed by either current repository |

Keep old versions in their original location rather than relabeling `0.7` as a
particular patch release. New README and Doxygen links use the new portal;
standalone development API artifacts link to its `latest/` guides. The portal
adapts those links to its actual version prefix when embedding the references.
Do not delete or rewrite historical release content to create redirects.

Core keeps all technical Markdown, public API comments, Doxygen inputs and
targets, native examples, and existing binding/CLI documentation. Only migrated
site configuration, Python site dependencies, hooks, theme and unused site asset
variants are removed. The README logo remains usable offline.

Portal [#6](https://github.com/OpenTLV/opentlv-docs/issues/6) transfers the
playground UI to `opentlv-docs`. Core retains a Markdown entry point and the
component-owned WebAssembly implementation, CMake target and tests. The portal
builds that target for its selected Core commit, verifies provenance and runs
the browser smoke test before publication. Its exact-path overlays support
older Core revisions that still contain the UI.
Merge and validate the portal integration before merging the Core UI removal.
The historical playground and its version history remain unchanged.

## Documenting new Core functionality

1. Update public C/C++ declarations and Doxygen comments with the implementation.
2. Update the canonical Core contract, relevant guide and native executable
   examples. Follow the [layout conventions](documentation-layout.md); keep
   component-specific behavior in the owning component's documentation.
3. Add or update the ownership inventory row for changed source boundaries.
   For a temporary exception, record its owner, the retained location and the
   migration condition rather than introducing a second canonical copy.
4. Update native support/build/example inventories when applicable. A Core-only
   change does not imply binding parity and must not fabricate binding examples
   or require unrelated binding implementation changes.
5. Run the relevant source/example inventory checks and Markdown lint. During
   this transition also follow the existing strict-site validation instructions;
   when the portal moves, site assembly/publication checks belong to its CI,
   while Core retains checks of its own sources and API generation.

Continue with the [contribution workflow](../../CONTRIBUTING.md) for commands and
the [extraction epic](https://github.com/OpenTLV/opentlv/issues/603) for migration
sequencing and preservation requirements.
