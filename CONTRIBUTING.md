# Contributing to OpenTLV

Report bugs and propose features through
[GitHub issues](https://github.com/OpenTLV/opentlv/issues), using the
Bug Report, Feature Request, or Documentation issue form. For bugs, include a
small reproducer, expected and actual behavior, compiler, build options, and
the version or commit used. Report suspected security vulnerabilities
privately instead, following [SECURITY.md](SECURITY.md).

All project spaces, including issues, pull requests, and reviews, are
governed by the [Code of Conduct](CODE_OF_CONDUCT.md).

For documentation changes, first follow the
[ownership rules](docs/development/documentation-ownership.md) and then the
[progressive learning conventions](docs/development/documentation-layout.md#progressive-learning-conventions).
Document new Core functionality alongside its implementation, public API comments
and native examples. Bindings and tools own their documentation; their current
co-location in Core is an explicit transitional exception. Portal assembly and
publication belong to `opentlv-docs`; the existing pipeline stays in place until
its replacement is validated. Core-only changes do not require unrelated binding
updates.
User onboarding starts at the [documentation homepage](docs/index.md); developer
onboarding continues with [the development workflow](docs/development/workflow.md).

Before opening a pull request:

- Follow the [core architectural rules](docs/concepts/architectural-rules.md).
- Add the standard SPDX license header to project source files (see
  [Source license headers](#source-license-headers)).
- Keep changes focused and preserve documented memory ownership and API boundaries.
- Update relevant documentation and add meaningful tests for behavior changes;
  document new or changed public API as described in
  [Public API documentation](#public-api-documentation).
- Build and run the applicable tests using the [getting-started guide](docs/getting-started/README.md#build-and-run-tests).
- Format C and C++ changes with clang-format, ideally via the pre-commit hooks
  (see below); CI rejects a pull request that isn't formatted.
- Record meaningful user-facing changes at the top of the appropriate `Unreleased`
  category in [CHANGELOG.md](CHANGELOG.md), including the issue reference.
- Describe the problem, resulting behavior, and verification in the pull request.

For substantial API or architecture changes, discuss the proposed scope in an
issue first. Repository-specific instructions are in [AGENTS.md](AGENTS.md).

## Source license headers

Start project C/C++ sources and headers with:

```c
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
```

Rust, JavaScript, TypeScript, Java and Go use the same comment syntax; Python
uses `#` and Lua uses `--`. Keep shebangs and Python encoding declarations
before the notice. C/C++ `.in` source templates also carry the notice so their
generated outputs inherit it. The complete MIT text remains in [LICENSE](LICENSE).

[scripts/check_license_headers.py](scripts/check_license_headers.py) owns the
covered extensions and explicit excluded paths, with reasons. Generated files
under `generated/`, vendored files under `vendor/` or `third_party/`, and
untracked build/dependency outputs are excluded. Do not replace third-party
license notices; document any additional exemption in the central policy.

Run `python scripts/check_license_headers.py` to check all tracked sources or
add `--fix` to insert missing notices while preserving file line endings.
Existing conflicting notices require manual review. The `license-headers`
pre-commit hook runs this same check locally and in the Static Analysis CI job.

For the branch model, what CI runs on pull requests, on `main` and on release
tags, and how to cut a release, see the
[development workflow](docs/development/workflow.md).

## Public API documentation

Update the relevant capability/support inventories and executable examples in
the same change as a public capability. Run `python scripts/check_doc_inventory.py`
and `python scripts/test_doc_inventory.py`; see the
[inventory maintenance rules](docs/development/documentation-layout.md#documentation-inventory-checks)
for sources of truth and the limits of automated checking.

The public headers ([tlv/include/tlv](tlv/include/tlv) and
[tlv++/include/tlv++](tlv++/include/tlv++)) are the authoritative source for
API contracts and are written as Doxygen comments, so a generated API
reference can be built from them. Every new or changed public declaration
follows the conventions below. The C and C++ API comments are rendered by
[Doxygen](#generate-the-c-api-reference) and can also be checked by the
compiler (see the end of this section).

**Format.** Use `/** ... */` blocks with a leading `@brief` line, a blank line,
then the details. Put the block directly above the declaration. Use
`/** ... */` above a member or enumerator (or `/**< ... */` after it) for
structure members, enum values and constants. Plain `/* ... */` and `//`
comments stay for implementation notes and are never used for public
contracts. Start each header with an `@file` block giving a one-line `@brief`,
and put any module-wide rules there.

**Tags**, in this order:

| Tag | Use |
| --- | --- |
| `@brief` | One sentence, imperative or descriptive, ending in a period. |
| `@param[in]`, `@param[out]`, `@param[in,out]` | Every parameter that is not obvious from its name and type. State the direction, whether `NULL` is allowed and when, valid ranges, and units (bytes, not elements). |
| `@return` | One line per distinct result code, naming the enumerator (`@return #TLV_ERR_NULL_ARG if ...`); use one line for success. |
| `@note` | Guarantees and clarifications, such as output preservation on failure. |
| `@warning` | Hazards the caller must act on, such as lifetime requirements, or callbacks that may leave a buffer modified on error. |
| `@see` | Related functions or types. |

**Describe the public contract, not the implementation.** Where they apply,
state each of the following explicitly:

- ownership and lifetime: who owns each buffer, and what must outlive a
  returned view, reader, writer or descriptor;
- allocation behavior, including that a function never allocates;
- zero-copy behavior: which outputs borrow input bytes;
- valid and invalid arguments, including `NULL` handling and overlap rules;
- error conditions, using the result-code names;
- output preservation on failure, such as `*written` left unchanged, a
  destination left unspecified, or a callback that may have modified it;
- reader and writer state changes, such as the position advancing only on
  success;
- native-size limitations, such as a 64-bit `tlv_size_t` narrowed to
  `size_t`;
- format- or standard-specific restrictions.

**Conventions.**

- Refer to code entities by name: `#TLV_OK` for constants, types and enumerators
  in the same header set, `function()` for functions (Doxygen links them), and
  backticks for parameters and other code. Cross-reference documentation files
  by path.
- Do not restate the signature. Skip `@param` only for parameters whose name and
  type make them obvious.
- A family of overloads that differ only by type (for example `_u8`, `_u16`,
  `_u32`, `_u64`) documents the first in full and uses `@copydetails` on the
  rest, each with its own `@brief`.
- Generated declarations must carry the same public documentation as handwritten
  declarations. Document explicit dictionary constants alongside their declarations.
- Preserve existing useful documentation when converting a comment; move its
  content into the structure above rather than rewriting it.
- Public C headers stay C99 and must not need Doxygen to compile.

To catch malformed comments (for example a `@param` that names a nonexistent
parameter), build the C headers with Clang's `-Wdocumentation`.

## Deterministic property tests

Enable `OPENTLV_BUILD_PROPERTY_TESTS=ON` and select the suite with
`ctest --test-dir build -L property --output-on-failure`. It uses the CLI
generator and public Tree Reader/Writer contracts to verify complete generated
streams, separately from unit, integration and fuzz testing. See the
[property suite guide](tests/property/README.md) for corpus identity, commit
seeds, optional-format coverage and failure reproduction.

## Generate the C API reference

Install [Doxygen](https://www.doxygen.nl/download.html) 1.9.1 or newer, CMake
and a C compiler. From the repository root, run:

```sh
cmake -S . -B build/docs -DOPENTLV_BUILD_DOCS=ON -DOPENTLV_BUILD_CXX=OFF -DOPENTLV_BUILD_TESTS=OFF -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build build/docs --target c-api-docs
```

Open `build/docs/docs/c-api/html/index.html`. No library compilation is
required for this target. `OPENTLV_BUILD_DOCS` defaults to `OFF`, so normal
builds do not require Doxygen; enabling it requires Doxygen at configure time.
Generated files stay in the build directory and must not be committed.

The reference covers `tlv/include/tlv` and the generated public configuration
and version headers. It includes all shipped C formats and standard-specific capabilities, even
when disabled in the configured library; configuration/version values describe
that build. Implementation sources and C++ headers are outside the input set.
The configuration is in [tools/docs/Doxyfile.in](tools/docs/Doxyfile.in).

API groups are defined in [tools/docs/c-api.dox](tools/docs/c-api.dox).
When adding a public header, use `@file` without a filename (some headers share
a basename), assign its file documentation with `@ingroup`, and surround its
declarations with `@addtogroup <group>` / `@{` / `@}` as in existing headers.
Keep includes outside that group block.

The [Documentation validation workflow](.github/workflows/docs-validation.yml) generates the same
reference, treats Doxygen warnings as errors and uploads the HTML as the
`c-api-reference` artifact. The standalone portal generates references from its
selected Core revision and publishes them under [Reference](docs/reference/c-api.md).

## Generate the C++ API reference

Use the same Doxygen configuration prerequisites and CMake configure command
as for the [C reference](#generate-the-c-api-reference), then run:

```sh
cmake --build build/docs --target cxx-api-docs
```

Open `build/docs/docs/cxx-api/html/index.html`. This target also generates the
C reference for cross-links; no C++ compiler or library compilation is needed,
and `OPENTLV_BUILD_CXX=OFF` does not disable reference generation.

The C++ namespace, class and header indexes cover only `tlv++/include/tlv++`.
Private members and implementation helpers are excluded. The reference shows
the C++11 fallback interfaces; its landing
page explains the standard-library aliases selected by newer language modes.
Ownership, lifetime and error contracts come from the public header comments.

[tools/docs/Doxyfile.cxx.in](tools/docs/Doxyfile.cxx.in) configures C++ extraction;
[tools/docs/Doxyfile.common.in](tools/docs/Doxyfile.common.in) shares HTML and
warning settings with C. The [landing page](tools/docs/cxx-api.dox) provides
reference navigation and compatibility notes, complementing conceptual docs.

The Documentation validation workflow generates both references with warnings as errors.
The `cxx-api-reference` artifact contains `cxx-api/html` and `c-api/html` as
siblings so links to C declarations work after extraction. Keep that layout
when copying the references.

The [standalone portal](https://github.com/OpenTLV/opentlv-docs) generates and
embeds both trees under `reference/api/`. Its README describes local previews
and verified external API artifacts. Core generation works without MkDocs or a
portal checkout. Link to API pages through the [C](docs/reference/c-api.md) and
[C++](docs/reference/cxx-api.md) entry pages, which also work on GitHub.

## Pre-commit hooks

[.pre-commit-config.yaml](.pre-commit-config.yaml) runs clang-format plus a
few whitespace/YAML/Markdown checks before each commit. Install
[pre-commit](https://pre-commit.com/) once per clone:

```sh
pip install pre-commit
pre-commit install
```

`git commit` then runs the hooks automatically and reformats or fixes what it
can; re-stage and commit again if it changed anything. Run them on the whole
tree (e.g. after changing `.clang-format`) with:

```sh
pre-commit run --all-files
```

[static-analysis.yml](.github/workflows/static-analysis.yml) runs the same
config in CI, so a clone with the hooks installed won't be surprised by CI.

## Code formatting

The `tlv` (C) and `tlv++` (C++) code, along with `tools/`, `tests/`,
`benchmarks/` and `examples/`, follows the style defined in
[.clang-format](.clang-format) (the root file, for tlv++ and everything built
on it) and [tlv/.clang-format](tlv/.clang-format) (an override for the pure C
layer). clang-format applies whichever of the two is nearest to a given file,
so both apply automatically; there's no `Language:`-based split in one shared
file because clang-format versions disagree on how to classify an ambiguous
`.h` file as C or C++, and clang-format 18 (still common) can't even parse a
`Language: C` section. Install the exact version pinned in
[.pre-commit-config.yaml](.pre-commit-config.yaml) (`rev:` under
`mirrors-clang-format`, currently 23.1.1 - `pip install clang-format==23.1.1`
also gets that same build) to avoid version-to-version formatting
differences, and, from a configured build directory, run:

```sh
cmake --build <build-dir> --target format        # reformat in place
cmake --build <build-dir> --target format-check   # check only, like CI
```

Both targets are opt-in and are skipped with a warning if clang-format isn't
found, so their absence never breaks a regular build.

## Static analysis (cppcheck)

[static-analysis.yml](.github/workflows/static-analysis.yml) also runs
[cppcheck](https://cppcheck.sourceforge.io/) over `tlv` (C) and `tlv++`
(C++) with `--enable=warning,style,performance,portability`. Reproduce it
locally by installing cppcheck, configuring a build directory, and building
the opt-in `cppcheck` target:

```sh
cmake -S . -B <build-dir> -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build <build-dir> --target cppcheck
```

Like `format`/`format-check`, the target is skipped with a warning (never
breaking a regular build) if cppcheck isn't found. Justified exceptions
(for example a limitation of cppcheck's own preprocessor, or intentional
API behavior a check flags) live in
[.cppcheck-suppressions](.cppcheck-suppressions), with a comment explaining
each one; prefer fixing a finding over suppressing it.

## Static analysis (clang-tidy)

[static-analysis.yml](.github/workflows/static-analysis.yml) also runs
[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) 18 over `tlv` (C) and
`tlv++` (C++). Check selection lives in [.clang-tidy](.clang-tidy) at the
repository root (the shared baseline for both languages) plus
[tlv/.clang-tidy](tlv/.clang-tidy) and [tlv++/.clang-tidy](tlv++/.clang-tidy),
which layer C- and C++-specific checks on top of it; clang-tidy resolves the
config for a given file by merging every `.clang-tidy` it finds walking up
from that file's own directory, so both apply automatically. Reproduce it
locally by installing clang-tidy, configuring a build directory with
`CMAKE_EXPORT_COMPILE_COMMANDS` enabled (clang-tidy needs the resulting
`compile_commands.json` to know each file's include paths and language
standard), and building the opt-in `clang-tidy` target:

```sh
cmake -S . -B <build-dir> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build <build-dir> --target clang-tidy
```

Like `format`/`format-check`/`cppcheck`, the target is skipped with a
warning (never breaking a regular build) if clang-tidy isn't found or
`CMAKE_EXPORT_COMPILE_COMMANDS` wasn't enabled (also requires
`OPENTLV_BUILD_CXX`/`OPENTLV_BUILD_TESTS`/`OPENTLV_BUILD_UNIT_TESTS`, all ON
by default). tlv++ is header-only, so there's no translation unit of its
own for clang-tidy to attach to; instead the target points clang-tidy at
`tests/unit/test_tlvpp.cpp`, which already includes and exercises
every tlv++ header with real instantiations (a synthetic TU that just
`#include`s the headers would compile but never instantiate their
templates, so clang-tidy would have nothing to analyze inside them).
Findings are still reported for that test file itself, so it has to stay
clang-tidy-clean too. Justified exceptions (for example a Clang Static
Analyzer false positive through an indirect function-pointer call it can't
resolve) are fixed with a
`NOLINT`/`NOLINTNEXTLINE`/`NOLINTBEGIN`-`NOLINTEND` comment explaining why,
right at the finding; prefer fixing a finding over suppressing it.

## C ABI compatibility

The **ABI Compatibility** CI job compares the `tlv` shared library with a
baseline. Run it locally with `python scripts/abi/check.py`; the baseline rules, what
counts as a breaking change and how to document one are in
[C ABI compatibility](docs/development/abi-compatibility.md).

## Documentation site

Core owns technical Markdown in [docs/](docs), public Doxygen comments and API
generation. The [standalone documentation repository](https://github.com/OpenTLV/opentlv-docs)
owns MkDocs, navigation, theme, shared site assets and publication. Install its
dependencies in an isolated environment and follow its README to preview this
Core checkout with `OPENTLV_DOCS_LOCAL=1`. Core itself needs no MkDocs packages.

Run Core source checks before opening a pull request:

```sh
pre-commit run markdownlint --all-files
python scripts/check_doc_examples.py
python scripts/test_doc_inventory.py
python scripts/check_doc_inventory.py
```

Generate the API references with the CMake targets above. The
[validation workflow](.github/workflows/docs-validation.yml) runs these source
checks and Doxygen with warnings as errors, and uploads both API references.
It has read-only repository permissions and never publishes a site.

See [where documentation belongs](docs/development/documentation-layout.md)
before adding a page. Keep the ownership inventory, GitHub index and source
links current. Add navigation entries in the portal and validate the assembled
site there with `python tools/docs/ci.py check` and `python -m mkdocs build --strict`.
The portal checks source-file links, page paths, anchors and navigation against
the selected Core revision. This validation does not require binding builds.

### Publishing

The [portal workflow](https://github.com/OpenTLV/opentlv-docs/actions/workflows/docs.yml)
publishes to <https://opentlv.github.io/opentlv-docs/>. A portal push updates
`latest` from its pinned Core revision. Release publication is requested in the
portal using a Core release tag; it retains full `X.Y.Z` versions and selects
the newest release as `stable`. Core tags and releases do not trigger or wait
for portal publication.

The [cutover procedure](docs/development/documentation-ownership.md#cutover-for-issue-608)
records deployment prerequisites, historical URL preservation, remaining
playground integration and the order of changes across both repositories.
