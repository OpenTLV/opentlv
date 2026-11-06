# Library and Core naming contract

This contract defines what **OpenTLV Library** and **Core** mean, records every
current public identifier with its scope, and sets the compatibility, breaking-change
and migration rules (#746, epic #702). The initial contract introduced no renames;
the inventory now records the subsequent implementation decisions. CMake fixes
(#753) and Library release-contract fixes (#754) precede the Library v0.9.0
distribution milestone (#716). Binding and tool migrations follow independently
against an approved, verified Library release.

Audience: maintainers of the Library, bindings, tools, Workspace and the portal.
Read the [functional module contracts](modules/README.md) first for the target
graph this contract names.

## Glossary

| Term | Meaning | Not |
| --- | --- | --- |
| OpenTLV Ecosystem | All projects in the `OpenTLV` organization | A package or repository |
| OpenTLV Library | The native C/C++ repository `OpenTLV/opentlv` and everything it distributes: sources, CMake package, verified binaries and, later, amalgamation | One module |
| Core | The foundational functional module: target `OpenTLV::Core`, headers `opentlv/core/`, sources `src/core/` | The whole Library, its releases, CI or repository |
| Module | An independently linkable functional target such as `OpenTLV::IO` or `OpenTLV::Query` | A cache option |
| Integration | A module-owned target joining two modules, such as `OpenTLV::QueryDocument` | A separate product |
| Builtin family | A protocol-family module under `opentlv/builtin/`, such as `OpenTLV::EMV` | Part of Core |
| Aggregate | The optional INTERFACE target `OpenTLV::tlv` linking every module | A compiled library |
| Binding / tool | A separate repository consuming a pinned Library release, such as `opentlv-rust` or `opentlv-cli` | Part of the Library |

Write "Library" (or "OpenTLV Library") for the repository, versions, releases,
CI, checkouts and distributions. Write "Core" only for the module. New text uses
"Library v0.8.0", not "Core v0.8.0". Historical text stays as written: closed
CHANGELOG sections, published release notes, tags and assets, and audit or
evidence records are never edited to apply this glossary.

## Identifier inventory

Scope says what each identifier actually refers to today. **Ambiguous** marks a
Library-wide identifier spelled with "core"; these are the only migration
candidates. Owner is the issue that decides any change.

### Library repository and CMake package

| Identifier | Current value | Scope | Decision | Owner |
| --- | --- | --- | --- | --- |
| Repository | `OpenTLV/opentlv` | Library | Keep; repository renames are out of scope | — |
| CMake project and package | `project(OpenTLV)`, `find_package(OpenTLV)` | Library | Keep | #747 |
| Package files | `lib/cmake/OpenTLV/OpenTLVConfig.cmake`, `OpenTLVConfigVersion.cmake` (`ExactVersion`), `OpenTLVModules.cmake` | Library | Keep | #747 |
| Export sets | `OpenTLVTargets.cmake`, `OpenTLV-<key>Targets.cmake` | Library / module | Keep | #747 |
| Target namespace | `OpenTLV::` | Library | Keep | #747 |
| Package components | Module export names, plus `tlv` for the aggregate | Module | Keep | #747 |
| Cache options | `OPENTLV_BUILD_*`, `OPENTLV_QUERY_*`, `OPENTLV_WARNINGS_AS_ERRORS` | Library | Keep | #747 |
| Amalgamation selection | `OPENTLV_AMALGAMATION_TARGETS`, a standalone-only cache entry of module export names, never in embedding builds | Library | Keep | #770 |
| Removed options | `OPENTLV_READER`, `OPENTLV_<FAMILY>` and the other rejected options | Library | Keep the configure-time rejection messages | #747 |
| Documentation install path | `share/doc/OpenTLV` | Library | Keep | #747 |
| Nested CMake project | Removed `project(tlv)` and its `tlv_SOURCE_DIR`, `tlv_BINARY_DIR`, `tlv_IS_TOP_LEVEL` cache entries | Build tree | Breaking embedding fix; use the `OpenTLV_*` project paths | #753 |

### Modules, headers and API

| Identifier | Current value | Scope | Decision | Owner |
| --- | --- | --- | --- | --- |
| Module targets | `OpenTLV::Core`, `IO`, `Document`, `Query`, `Schema`, `Codec`, `Generator`, `ASN1`, `EMV`, `Bluetooth`, `NFC`, `DHCP`, `LLDP` and integrations | Module | Keep; `OpenTLV::Core` is the Core module only | #747 |
| Aggregate target | `OpenTLV::tlv` (INTERFACE, no archive) | Library | Keep; see below | #747 |
| Aggregate build target | `opentlv_tlv`, `EXPORT_NAME tlv`; deprecated `tlv` alias only in standalone builds | Build tree | Link `OpenTLV::tlv`; embedding never reserves `tlv` | #753, #754 |
| Removed targets and paths | Monolithic `tlv` library, `tlv++` target, `tlv/` and `tlv++/` include paths | — | Removed in #654 and #733; never reused | — |
| Build targets and binaries | `opentlv_<key>`, e.g. `libopentlv_core.so`, `opentlv_core.dll` | Module | Keep; direct consumers link these file names | #747, #748 |
| Export macros | `OPENTLV_<KEY>_API`, `OPENTLV_<KEY>_STATIC_DEFINE`, e.g. `OPENTLV_CORE_API` | Module | Keep | — |
| Include root | `opentlv/<module>/`, `opentlv/builtin/<family>/` | Library / module | Keep; changed in #733 | — |
| Umbrella headers | `opentlv/tlv.h`, `opentlv/tlv.hpp`, `opentlv/native.hpp` | Library | Keep | — |
| C API | `tlv_` functions and types, `TLV_` constants | Library | Keep; API renames are out of scope | — |
| C++ API | namespace `tlv` | Library | Keep | — |
| Header guards | `OPENTLV_*_H`, `OPENTLV_*_HPP` | Header | Keep; not an interface | — |
| Library version | `opentlv/core/version.h`: `OPENTLV_VERSION_*`, `OPENTLV_GIT_*`, `tlv_version_*()` | Library version owned by the Core module | Keep; the values identify the Library release | — |

`OpenTLV::tlv` is an INTERFACE aggregate that links the functional modules. It
is not the former monolithic `tlv` library, which no longer exists: the aggregate
has no archive or shared library of its own. The separate `tlv++` target and the
historical `tlv/` and `tlv++/` include paths were removed without aliases.

The Core module providing the Library version is deliberate: every consumer links
Core. It does not make Core a synonym for the Library.

### Releases, CI and evidence

| Identifier | Current value | Scope | Decision | Owner |
| --- | --- | --- | --- | --- |
| Tags | `vX.Y.Z` (historical unprefixed tags remain readable) | Library | Keep | #748 |
| Release title | `OpenTLV X.Y.Z` | Library | Keep | #748 |
| Assets | `OpenTLV-X.Y.Z-source.tar.gz`, `OpenTLV-X.Y.Z-<os>-x64-<linkage>.{tar.gz,zip}`, `<id>.json`, `release-manifest.json`, `SHA256SUMS`, `performance*` | Library | Keep | #748 |
| Archive contents | Prefix `OpenTLV-X.Y.Z/`, metadata `opentlv-source.txt` | Library | Keep | #748 |
| CPack release package | `OpenTLV-<version>-<os>-<architecture>-<linkage>`, also the top directory inside each binary asset | Library | Contract 2 from v0.9.0; other compilers retain a discriminator; see the [migration](#artifact-contract-2) | #748, #754 |
| Workflow files | `core-pr.yml`, `core-main.yml`, `core-full.yml`, `core-checks.yml`, `core-release.yml` | **Ambiguous** | Keep: binding verifiers pin `core-release.yml` as the attestation signer and expect these file names in release evidence | #748 |
| Workflow and artifact names | `Core Pull Request CI`, `Core Release`, `core-quality-full-*`, `core-release-timings-*` | **Ambiguous** | Display names may change in #748; artifact names that verifiers read follow the alias rules | #748 |
| Release scripts | `scripts/core_release.py`, `scripts/core_performance.py` | **Ambiguous** | Internal; rename only together with their workflows | #748 |

### Downstream identifiers for the Library

These name the whole Library but are owned by the consuming repository. The
Library does not read them.

| Identifier | Current value | Scope | Owner |
| --- | --- | --- | --- |
| FetchContent dependency | `opentlv_core` and its `FETCHCONTENT_SOURCE_DIR_OPENTLV_CORE` override | **Ambiguous** | opentlv-cli#27, opentlv-python#58, opentlv-rust#49, opentlv-lua#58, opentlv-go#34, opentlv-js#48 |
| Local checkout overrides | `OPENTLV_CORE_SOURCE`, `OPENTLV_CORE_SOURCE_DIR`, `OPENTLV_CORE_REF` | **Ambiguous** | Each binding; opentlv-workspace#56 |
| Release verifiers and pins | `verified_core.py`, `Core.cmake`, `core-artifact.json`, `core-source.json`, `core-features.json`, `CORE_CHECKS`, `CORE_RELEASE`, `CORE_REPOSITORY`, `opentlv-core` paths | **Ambiguous** | Each binding; opentlv-workspace#56 |
| Prose | "OpenTLV Core", "Core v0.8.0", "Core repository", "Core release", "Core CI" | **Ambiguous** | opentlv-docs#174 and each repository |

The Core module's build target is also called `opentlv_core`. A FetchContent
name creates no target, so nothing collides today, but the two spellings mean
different things and must not be read as one identity.

## Recorded decisions

- **CMake package:** `OpenTLV` already names the Library. Keep the package name,
  file names, namespace, `ExactVersion` compatibility and component names. #747
  verifies source, FetchContent and installed consumers against this inventory.
- **Exports:** Keep every module and integration target. `OpenTLV::Core` never
  grows to mean the Library and is never repointed. Keep `OpenTLV::tlv`. A
  Library-named aggregate is optional; if #747 adds one, the candidate is
  `OpenTLV::OpenTLV` (the `<Package>::<Package>` convention) as an alias of
  `OpenTLV::tlv`, never a name containing `Core`.
- **Dependencies:** The canonical FetchContent name for the Library is `opentlv`,
  so its override is `FETCHCONTENT_SOURCE_DIR_OPENTLV`. New Library-wide
  variables use the `OPENTLV_LIBRARY_` prefix, e.g. `OPENTLV_LIBRARY_SOURCE`.
  Existing `opentlv_core` and `OPENTLV_CORE_*` spellings remain accepted aliases
  in each downstream repository under the rules below.
- **Release artifacts:** Asset, archive, manifest and checksum names already use
  the Library identity. Keep them. #748 decides the workflow display names and
  any added manifest fields; it must keep verifier-visible workflow paths or run
  old and new signer identities side by side.
- **Amalgamation (#701):** Generated sources are a Library distribution form and
  use Library identity (`opentlv`, `OpenTLV`) from their first release, with no
  legacy aliases. A per-module amalgamation uses that module's export name;
  "core" appears only when the artifact contains only the Core module. Exact
  target, component and asset names are set in #701 and #748. The
  [amalgamation contract](amalgamation.md#names) (#741) sets the build target
  and file names; #742 adds the opt-in `Amalgamation` install component. The
  release asset remains held for #744.

## CMake identity audit

The first part of #747 audits the CMake identities above against the current
Library, before v0.9.0. It renames nothing and adds no alias.
`scripts/check_cmake_integration.py` now fails when any of these identities
changes, in every consumer mode:

| Mode | Consumer spelling | Checked |
| --- | --- | --- |
| `package` | `find_package(OpenTLV <x.y.z> EXACT CONFIG REQUIRED COMPONENTS Core)` | Imported from the relocated prefix; no other contract target is loaded |
| `package-all` | `find_package(OpenTLV <x.y.z> EXACT CONFIG REQUIRED)` | Every contract target is imported and its `OpenTLV_<name>_FOUND` is set |
| `fetch` | `FetchContent_Declare(opentlv_core ...)`, `FETCHCONTENT_SOURCE_DIR_OPENTLV_CORE` | Complete embedded build; aliases and embedded footprint |
| `fetch-canonical` | `FetchContent_Declare(opentlv ...)`, `FETCHCONTENT_SOURCE_DIR_OPENTLV` | Aliases and embedded footprint; subsequent discovery resolves to the embedded Library |
| `subdirectory` | `add_subdirectory(<checkout> opentlv)` | Aliases and embedded footprint |
| `fetch-package` | `opentlv_core` with `FIND_PACKAGE_ARGS <x.y.z> EXACT CONFIG NAMES OpenTLV` (CMake 3.24+) | Every contract target is imported |
| `fetch-package-canonical` | `opentlv` with the same package arguments | Every contract target is imported |
| `build-tree` | `OpenTLV_DIR=<build>` | Every contract target is imported from build-tree exports |
| `fetch-collision`, `fetch-collision-after` | Legacy FetchContent with an application-owned `tlv` target declared before or after embedding | Application target is preserved; `OpenTLV::tlv` aliases `opentlv_tlv` |
| `subdirectory-after` | `add_subdirectory()` followed by an application-owned `tlv` target | Both declaration orders work and link the canonical aggregate |

In every mode, each public target from the
[module contract](modules/ownership.json) resolves to the same `OpenTLV::` name:
an alias of `opentlv_<key>` (or `opentlv_tlv` for the aggregate) from source, an imported
target from the package. The check also pins:

- The package directory holds exactly `OpenTLVConfig.cmake`,
  `OpenTLVConfigVersion.cmake`, `OpenTLVModules.cmake`, `OpenTLVTargets.cmake`
  and one `OpenTLV-<key>Targets.cmake` per component, plus their
  per-configuration files. The component registry matches the module contract.
- `ExactVersion` rejects another patch version, and documentation is installed
  under `share/doc/OpenTLV`.
- Every removed option still fails at configure time with its message.
- An embedding build creates only the `opentlv_tlv` and `opentlv_<key>` targets.
  The deprecated `tlv` alias is standalone-only. It adds
  only these cache entries: the `OPENTLV_*` options, the `OpenTLV_*`
  project variables, and entries owned by CMake's GNUInstallDirs,
  GenerateExportHeader and FetchContent modules.

Unit tests in `scripts/test_module_contracts.py` keep the option list equal to
the `option()` declarations and cover every failure these checks report.

Both FetchContent names work with the current Library. The Library, v0.8.0
included, never reads the dependency name, so the rename itself needs no Library
change.

<!-- markdownlint-disable-next-line MD033 -->
<a id="findings-for-the-post-v090-step"></a>

### CMake discovery and embedding fixes for v0.9.0

The #747 audit recorded these five findings without changing behavior. #753
resolves them before v0.9.0 as release-preparation work under #702, including the
necessary embedding migration below. Verified downstream v0.8.0 pins remain
unchanged until each consumer deliberately migrates.

1. **Build-tree discovery:** `export(EXPORT ...)` now supplies the module and
   aggregate exports beside the build-tree config. Installed exports keep their
   names and relocation behavior. Build-tree exports require the original source
   and build directories.
2. **Components:** the config reads `CMAKE_FIND_PACKAGE_NAME` and checks that
   spelling's required/optional flags before importing targets. It exposes
   metadata under both that spelling and `OpenTLV`. Unknown required components
   reject the package; optional unknown components stay unavailable even if an
   unrelated target happens to use that name. A failed package probe imports no
   partial target set before FetchContent can fall back to source population.
3. **One Library identity:** targets record their package directory. Configs
   reject targets from another installation or embedded build. Embedding supplies
   package directory hints to forwarding configs on CMake 3.16 and redirects for
   both FetchContent names on CMake 3.24+, including plain declarations and
   `OVERRIDE_FIND_PACKAGE`.
4. **Versioned redirects:** redirects use the same `ExactVersion` file and
   component-aware config as the package. `OpenTLV_VERSION` and the dependency
   spelling's version identify the selected Library, including unversioned calls.
5. **Embedding names:** the build target is `opentlv_tlv` with `EXPORT_NAME tlv`.
   Public `OpenTLV::tlv` and `OpenTLVTargets.cmake` are unchanged. The old source
   spelling `tlv` is a deprecated alias only in standalone builds; it still means
   the aggregate there. Embedding never creates that alias, so the application
   can define `tlv` before or after `add_subdirectory()` or FetchContent.
   This embedding compatibility correction ships before the first 0.9 release;
   no released deprecation period is shortened. Migrate links to `OpenTLV::tlv`.
   Future removal of the standalone alias follows the two-minor-release rule.
   The internal Library binary directory is `opentlv-library`, with policies
   established by the root CMake project rather than a redundant nested minimum.
   Removing `project(tlv)` and its cache variables is an intentional breaking
   change: use `OpenTLV_SOURCE_DIR` and `OpenTLV_BINARY_DIR` for Library paths and
   the application's own project identity for top-level decisions.

Package discovery updates only the requested spelling's `_FOUND`,
`_NOT_FOUND_MESSAGE` and component results. A failed `opentlv_core` or `opentlv`
request does not invalidate an earlier successful `OpenTLV` request (and vice
versa). A failed request through the *same* name has ordinary CMake semantics:
its `_FOUND` becomes false, but existing targets remain intact. Successful
discovery supplies canonical `OpenTLV_VERSION`; failed alternative requests do
not replace that metadata. Required components are validated before importing
any target, and target-origin checks reject mixed installations. Normal lookup
uses CMake's version check once; forwarding configs perform the same check
internally once, avoiding duplicate `ExactVersion` range warnings.

`scripts/check_cmake_discovery.py`, run by the consumer gate, covers positive and
negative component/version requests, repeated discovery, conflicting installed
and embedded versions, and both dependency spellings. The gate also runs source,
build-tree and relocated consumers on CMake 3.16; CMake 3.24+ adds redirects and
FetchContent package lookup. Local results do not replace required hosted CI.

## Release artifact audit

The first part of #748 audits the release identities against the published
Library v0.8.0 and the current `core-release.yml`, before v0.9.0. It renames
nothing. The audit below preserves that baseline; [contract 2](#artifact-contract-2)
records the #754 implementation for new releases. Producer, CPack and workflow
tests require the identities and fields of the applicable contract.

### Artifact contract

This is version 1 of the artifact contract. It describes v0.8.0, the first
release published by `core-release.yml`. Version 2 applies starting with v0.9.0,
including its prereleases, under the [breaking-change policy](#breaking-change-policy). `X.Y.Z` is the
version without the tag's `v`, including any prerelease suffix such as `-rc.1`.
`<id>` is one of `linux-x64-static`, `linux-x64-shared`, `windows-x64-static`
and `windows-x64-shared`.

| Artifact | Name | Identity and contents |
| --- | --- | --- |
| Tag | `vX.Y.Z` | Lightweight tag on the release commit, pushed by the publish job |
| Release | Title `OpenTLV X.Y.Z` | Generated notes; a prerelease when the version has a suffix |
| Source archive | `OpenTLV-X.Y.Z-source.tar.gz` | Top directory `OpenTLV-X.Y.Z/`, with `opentlv-source.txt` holding `version=X.Y.Z` and `commit=<sha>` |
| Binary archives | `OpenTLV-X.Y.Z-<id>.tar.gz` on Linux, `.zip` on Windows | The verified CPack archive, unchanged: top directory `OpenTLV-X.Y.Z-<system>-<processor>-<compiler>/`, e.g. `OpenTLV-0.8.0-Linux-x86_64-GNU/` |
| Binary records | `<id>.json` | Schema 1: `kind`, `id`, `version`, `commit`, `filename`, `sha256`, `os`, `architecture`, `linkage`, `build_type`, `compiler`, `msvc_runtime`, `libc`, `runner_image`, `options` |
| Performance | `performance.json`, `performance.md`, `performance-data.tar.gz` | Advisory comparison with the previous stable release |
| Manifest | `release-manifest.json` | Schema 2: `version` (no `v`), `commit`, `candidate`, `quality`, `performance` and one `artifacts` record per archive and performance file |
| Checksums | `SHA256SUMS` | `<sha256>  <name>` for every other asset |
| Attestations | One per asset | Signer workflow `OpenTLV/opentlv/.github/workflows/core-release.yml`, source digest = release commit |

The verifiers in opentlv-cli, opentlv-python, opentlv-rust and opentlv-lua
(`verified_core.py`) read only part of this contract, but that part must not
change without the breaking-change policy:

- the signer workflow path and the release commit as source digest;
- `release-manifest.json` and `SHA256SUMS` by their exact names; every other
  asset by the `filename` its manifest record gives;
- in the manifest: `schema`, `version`, `commit`, the `candidate` identity, the
  `quality` identity and checks, and `quality.configuration`, whose workflow and
  configuration file sets opentlv-python and opentlv-rust require exactly and
  compare file by file with the Verified Source;
- in each artifact record: `kind`, `filename`, `sha256`, `version`, `commit`,
  and for binaries `id`, `os`, `architecture`, `linkage`, `build_type`,
  `compiler`, `msvc_runtime`, `libc` and `options`;
- the source archive's top directory and `opentlv-source.txt`.

Adding a manifest or record field is not breaking, because no verifier rejects
unknown fields. The tests therefore require every field listed above but accept
additions. The published file set stays exact.

No verifier reads the release title or the binary archives' top directory: they
locate package files by searching the extracted archive. Workflow display names
(`Core Release`), CI artifact names (`release-*-<attempt>`,
`core-quality-<tier>-<sha>-<attempt>`, `core-timings-*`,
`core-release-timings-*`) and the run-local `core-quality.json` and
`release-candidate.json` file names are internal. They are pinned so that a
change is deliberate, and may change without an alias.

### Historical releases

Releases before v0.8.0 use unprefixed tags (`0.0.1` to `0.7.1`), and only
`0.3.0` has assets: CPack archives named `OpenTLV-0.3.0-Linux-x86_64-GNU.tar.gz`,
`OpenTLV-0.3.0-Linux-x86_64-Clang.tar.gz` and
`OpenTLV-0.3.0-Windows-AMD64-MSVC.zip`, without checksums, manifest or
attestations. The `0.4.0` title contains two spaces. These are published
identities: rule 5 keeps them as they are. The performance baseline already
accepts both tag forms.

### Decisions from the audit

- **Keep every name in the contract.** Asset, archive and record names already
  use the Library identity or none; nothing in them says "core".
- **Release title, fixed now.** The publish job created the title
  `OpenTLV vX.Y.Z`, while this contract and the published v0.8.0 release read
  `OpenTLV 0.8.0`. The job now removes the tag's `v`. No verifier reads the
  title, so this is not a breaking change.
- **New assets.** Any later asset, including amalgamation (#701), is named
  `OpenTLV-X.Y.Z-<name>`, gets its own manifest record `kind`, a `SHA256SUMS`
  line and an attestation from the same signer. Unprefixed names such as
  `<id>.json` are not extended.

### Findings for release artifacts

At the audit baseline none of these was fixed except the release title above.
The [contract 2 decisions](#artifact-contract-2) below resolve the Library-owned
work in #754; required hosted release validation remains a gate for epic #702
and release #716. Binding verifier changes
remain owned by the binding issues named; moving their pins requires validation
against the approved Library release assets.

1. **Binary archives carry a second identity.** The top directory inside each
   binary asset is the CPack name: CMake's processor spelling (`x86_64`,
   `AMD64`) and compiler ID, and no linkage. The asset name says `x64` and the
   linkage, so the static and shared archives of one system share a top
   directory and merge silently when extracted into one directory. A top
   directory `OpenTLV-X.Y.Z-<id>/` would remove the second identity. Verifiers
   do not depend on it, but it is an inventoried archive prefix, so the
   breaking-change policy applies.
2. **The next release's evidence fails today's verifiers.** The manifest
   schema is still 2, but its content no longer matches what the verifiers
   expect from v0.8.0:
   - v0.8.0 records the configuration files `CMakeLists.txt`,
     `cmake/compiler.cmake`, `cmake/capability_options.cmake`,
     `cmake/format_options.cmake` and three scripts. Since the module
     extraction (#652, #654) the producer records `cmake/modules.cmake`,
     `cmake/library/CMakeLists.txt` and `cmake/library/modules.cmake` instead.
     opentlv-python and opentlv-rust require the v0.8.0 set exactly and reject
     the manifest.
   - Binary records copy the `OPENTLV_*` cache entries into `options`.
     opentlv-python and opentlv-lua require the removed options
     `OPENTLV_READER`, `OPENTLV_QUERY` and the other module and family options
     to be `ON` (`core-features.json`), so no new binary matches them.
   - opentlv-python, opentlv-rust and opentlv-lua select a static Linux binary
     only when its record has `position_independent_code: true`. No record has
     ever contained that field.
   - The opentlv-rust binary mode looks for `tlv.lib` or `libtlv.a`,
     `tlv/version.h` and `tlv/config.h`, which #654 and #733 removed.

   The module and header changes behind the configuration, option and layout
   mismatches are already **Breaking:** entries; their effect on release
   verification is not recorded anywhere else. The binding
   issues (opentlv-python#58, opentlv-rust#49, opentlv-lua#58) must update
   their verifiers before moving their pins. Before a binding publishes against
   v0.9.0, its verifier must pass in that binding's CI against the actual
   v0.9.0 assets, not against fixtures. The Library CI does not run the binding
   verifiers. For the Library,
   decide whether a changed configuration set requires a new manifest schema,
   whether verifiers should accept any set whose files all match the Verified
   Source (they compare each file already), and whether binary records should
   describe the modules they contain and their position independence.
3. **Generic asset names.** `<id>.json`, `performance.*`,
   `release-manifest.json` and `SHA256SUMS` name neither the Library nor the
   version, and every binding release publishes its own `release-manifest.json`
   and `SHA256SUMS`. Assets of two releases downloaded into one directory
   overwrite each other. Verifiers read the manifest and checksums by these
   names, so they stay; prefixed names could only be added beside them.
4. **The manifest does not state the contract.** The Library identity appears
   only as `candidate.repository` and `quality.repository`, and no field names
   the artifact contract version. A verifier cannot tell a renamed artifact set
   from an unknown one. Top-level fields such as `library` and
   `artifact_contract` would be additions, which no current verifier rejects.

### Artifact contract 2

Starting with v0.9.0 (including prereleases), the producer emits
`library: "OpenTLV"` and `artifact_contract: 2` in `release-manifest.json`.
The manifest keeps schema 2, Full CI evidence keeps schema 2 and binary records
keep schema 1: their existing fields retain their meaning. Contract 2 versions
the new archive namespace and modular evidence requirements. An absent
`artifact_contract` identifies the historical contract 1, not contract 2.
Consumers must explicitly support a contract before selecting its artifacts.

Before its first publication, #744 extends contract 2 with the required
`amalgamation` kind, `OpenTLV-<version>-amalgamation.tar.gz` (same stem for its
single top directory), and companion `amalgamation.json`. This deliberately
updates the exact approved artifact set; it does not change any published
contract 1 release. Downstream contract 2 migrations must accept and verify
these additional assets and the new configuration sources listed below.
Source/binary consumers continue selecting their own explicit kind; they must
not mistake the amalgamation for a Verified Source or a prebuilt binary.
The [amalgamation release contract](amalgamation.md#release-archive-and-verification)
defines its required record fields, fixed Query configuration, reproducibility,
license, consumer and tested-byte checks. The record is embedded unchanged in
the release manifest and both assets are checksummed and attested.

#### Archive migration

The binary filename and its single top directory now share the stem
`OpenTLV-X.Y.Z-<id>`, e.g. `OpenTLV-0.9.0-rc.1-linux-x64-static/` and
`OpenTLV-0.9.0-rc.1-linux-x64-shared/`. The source archive remains unchanged.
CPack uses lowercase system/processor names, normalizes `AMD64`, `x86_64` and
`x64` to `x64`, and appends `static` or `shared`. Compiler identity remains in
the binary record; the approved release matrix has one compiler per OS.
Local CPack builds outside the Linux/GNU and Windows/MSVC compiler pairs insert
a lowercase compiler discriminator before linkage, e.g.
`OpenTLV-0.9.0-linux-x64-clang-static`, to avoid merging different toolchains.
Different versions of the same compiler still need separate download directories.

This is a breaking archive-prefix change in the 0.9 minor release. Archive
consumers must use the new required binary `archive_root` field or discover
the single root, rather than reconstructing the old compiler-based name.
There is deliberately no old-directory alias: duplicating or linking the old
root would restore the static/shared collision. This is the contract's narrow
exception to the alias-first rule. Published v0.8.0 archives and all older
tags, records, hashes and attestations stay immutable. Regression tests reject
the old root for newly produced archives and extract stable/prerelease static
and shared packages together, retaining each package's own contents.

#### Evidence schema and binary metadata

`quality.configuration.schema: 1` identifies the modular configuration evidence.
Absence of that field identifies the legacy unversioned evidence. Its required
fields are `schema`, `inputs`, `configuration_sources` and `workflows`.
`configuration_sources` maps repository-relative file names to SHA-256 digests;
`workflows` maps workflow basenames to `sha256` and exact `definition` text.
All paths must stay within the Verified Source, and every digest and definition
must match that source. Required modular sources are:

- `CMakeLists.txt`, `cmake/compiler.cmake`, `cmake/modules.cmake`;
- `cmake/library/CMakeLists.txt`, `cmake/library/modules.cmake`,
  `cmake/library/cxx.cmake`;
- `cmake/install.cmake`, `cmake/packaging.cmake`, `cmake/version.cmake`,
  `cmake/OpenTLVConfig.cmake.in`;
- `scripts/core_release.py`, `scripts/check_package.py`,
  `scripts/archive_safety.py`, `scripts/check_cmake_integration.py`,
  `scripts/check_cmake_discovery.py`, `scripts/check_capabilities.py`,
  `scripts/abi/check.py`;
- `scripts/check_distribution_equivalence.py`,
  `tests/amalgamation/equivalence/CMakeLists.txt`, `tests/fuzz/targets.cmake`.
- `scripts/amalgamation_release.py`, `scripts/check_amalgamation_consumers.py`,
  `cmake/amalgamation.cmake`, `cmake/amalgamation/generate.cmake`,
  `cmake/amalgamation/export.h.in`, `cmake/amalgamation/selection.cmake`.

The required Full release workflow set remains the set pinned by
`test_release_evidence_records_the_files_that_verifiers_compare` in
`scripts/test_core_release.py`. The producer compares the entire configuration
against the release checkout, including the resolved release-tag input, before
publishing. Binding verifiers must enforce these required sets, verify every
additional source/workflow against the same Verified Source, and reject missing
required files. They must not accept an arbitrary self-consistent subset.
Adding evidence files is extensible; removing required files or changing their
meaning requires a new configuration schema and a downstream migration.

Binary records add these required contract 2 fields:

| Field | Meaning |
| --- | --- |
| `artifact_contract` | Integer `2` |
| `archive_root` | The single extraction directory, without a trailing slash |
| `modules` | Sorted records for registered module and integration targets; each has `name` (CMake component), `target` (`OpenTLV::...`), `linkage` and `position_independent_code` |
| `features` | Boolean `OPENTLV_QUERY_FRONTEND` and `OPENTLV_QUERY_SET_OPERATIONS` values |
| `position_independent_code` | Boolean: true only when all compiled modules have CMake's target `POSITION_INDEPENDENT_CODE` enabled |

Per-module `linkage` is `static`, `shared` or `interface`; PIC is a Boolean for
compiled modules and `null` for an interface. These values come from generated
target properties, including overrides, rather than an inference from the cache.
The `modules` list includes integration facades, but excludes the optional
`OpenTLV::tlv` aggregate because it is not a module. The existing top-level
`linkage` describes the compiled libraries. All approved release builds enable
PIC, including Linux static packages, and the producer rejects any compiled
module without it. On Windows the field records the CMake target setting;
it does not claim ELF flags or cross-platform binary compatibility.

`options` still records actual `OPENTLV_*` cache entries for build provenance.
It is not a module-availability map. Consumers select modules through `modules`
and optional Query behavior through `features`. Removed module/family options,
monolithic `tlv` archives and legacy headers are not recreated. The owning
binding must link the modular dependency closure and use `opentlv/<module>/`
headers. The header-only C++ facades remain installed independently of the
development-only `OPENTLV_BUILD_CXX` setting.

#### Extensibility, checksums and provenance

Before extraction, the package verifier and release producer validate the entire
archive namespace. TAR permits only regular files, directories and relative
symlinks whose complete resolution stays inside the declared root and ends at an
existing file or directory. Versioned `.so` symlink chains are supported. Absolute
paths, entry traversal, hardlinks, sparse files, devices, FIFOs and other special
types are rejected. Symlink targets are resolved from the link's containing
directory, following other links before interpreting `..`; escaping, cyclic or
dangling chains fail. No entry may be nested below a symlink, even a safe one.
Duplicate normalized paths and file/directory conflicts fail regardless of
archive order. ZIP permits only regular files and directories, rejects links
and Windows path aliases, and checks CRCs. Truncated or malformed archives fail
closed. Validation does not extract malicious inputs to test their safety.

Every field in the version 1 tables remains required, with the additions above
required for contract 2. Readers may ignore unknown object fields. New optional
fields do not require a schema bump; deleting, retyping or redefining required
fields does. An unknown manifest schema, artifact contract or configuration
schema must fail verification until the reader supports it. Asset names,
the approved binary IDs and required artifact kinds remain exact for contract
2; adding another published kind or filename requires a deliberate contract
update, including the owning consumers' selection and validation rules.

`release-manifest.json`, `SHA256SUMS`, binary record filenames and performance
filenames stay unchanged. Every other published file, including the manifest
and binary records, has exactly one SHA-256 line in `SHA256SUMS`; the checksum
file does not checksum itself. Each published file, including `SHA256SUMS`,
is attested by `OpenTLV/opentlv/.github/workflows/core-release.yml` with the
release commit as source digest. Candidate and quality repository, commit, run
ID and attempt must match the same release run, and all Full checks must pass.

Prefixed manifest/checksum companions were evaluated and are deliberately
omitted in contract 2. They would not make co-download safe: binary records and
performance files would still collide, while duplicate checksum entry points
would add another synchronization and provenance obligation. Download each
repository/tag into its own directory. This resolves finding 3 by retaining the
verifier-visible paths and documenting the limitation, rather than promising
that all assets can share a directory. Binary extraction coexistence is tested
separately.

#### Tracking and downstream handoff

For #702 and audit #748, findings 1, 2 and 4 have Library-side implementations
in #754; finding 3 has the explicit no-companion decision above. Contract 1 is
retained as historical evidence. These decisions do not establish hosted PR,
Full CI, release validation or publication success.

The existing verifier migrations belong to
[opentlv-python#58](https://github.com/OpenTLV/opentlv-python/issues/58),
[opentlv-rust#49](https://github.com/OpenTLV/opentlv-rust/issues/49) and
[opentlv-lua#58](https://github.com/OpenTLV/opentlv-lua/issues/58). Each must
support contract 2, modular configuration evidence, module selection, PIC and
the new headers/libraries, while retaining verification of its pinned v0.8.0
assets until migration. Each binding must record its own CI run against the
actual approved v0.9.0 assets and a clean consumer before publishing against
them. Fixtures and Library CI are insufficient. Library CI owns producer,
archive, native consumer and workflow checks; it does not run every binding
verifier. #702/#748 tracking must retain those outstanding evidence gates;
only release-risk approval recorded in #702 can waive required release work.

## Compatibility rules

1. **No silent meaning change.** An existing identifier keeps its current
   meaning. Rename by adding a new identifier, never by redefining an old one.
2. **Alias first.** A rename ships the new name and keeps the old one as a
   deprecated alias in the same Library release. CMake 3.16 package
   configurations cannot alias a non-global imported target, so an installed
   alias is an `INTERFACE IMPORTED` target linking the original.
3. **One source of truth.** When an old and a new variable are both set to
   different values, fail with a message naming both. Never pick one silently.
4. **Deprecation period.** Remove an alias no earlier than the second Library
   minor release after the one that introduced its replacement, and only after
   every owning downstream issue has migrated or explicitly waived it.
5. **Published identities are immutable.** Tags, release assets, checksums,
   attestations, archive prefixes and historical documentation are never
   renamed, rebuilt or re-uploaded. New names apply to new releases only.
6. **Separate changes.** Naming-only changes contain no behavior, API, ABI or
   build-option change. Keep discovery and embedding fixes distinct from
   unrelated renames. A build-identifier migration needed to resolve an audited
   collision must preserve exported identities and document its compatibility
   and breaking-change impact.

## Breaking-change policy

Removing, renaming or redefining any identifier in the inventory is breaking,
as is changing an asset filename pattern, an archive prefix, a verifier-visible
workflow path or the package configuration file names. A breaking naming change
requires:

- a **Breaking:** CHANGELOG entry with the old name, the new name and the
  migration step;
- a Library minor release under the project's 0.x Semantic Versioning, not a patch;
- tested old and new spellings in source, FetchContent and installed consumers
  while the alias exists;
- linked downstream issues before the change merges.

Adding a new alias, a new identifier or a documentation-only glossary
correction is not breaking.

## Migration sequence

```text
#746 contract -> #747 / #748 audits -> #753 CMake / #754 artifacts
  -> v0.9.0 (#716) -> bindings and tools
  -> Workspace -> documentation -> alias removal
```

1. Approve this contract (#746). No identifier changes.
2. Audit CMake identities (#747) and release artifact identities (#748).
3. Resolve the Library findings in #753 and #754 before v0.9.0, preserving public
   identities or applying the documented alias and breaking-change rules.
4. Run the required PR, Full and release gates, then publish verified Library
   v0.9.0 artifacts (#716). Unresolved #753 or #754 acceptance work requires an
   explicit release-risk waiver in #702; audit completion alone is insufficient.
5. Migrate each binding and tool separately, pinned to the approved release, through its
   own CI. Bindings keep their verified v0.8.0 pins until they deliberately move.
6. Update Workspace checkout and override names (opentlv-workspace#56).
7. Update current documentation (opentlv-docs#174 and each repository).
8. Remove deprecated aliases in a separate breaking issue once rule 4 is met.

## Naming gate

Every pull request that adds or changes a public identifier answers these in its
description, and reviewers reject a change that cannot:

- Is the identifier listed in this inventory, or added to it in the same change?
- Does "Core" refer only to the Core module?
- Is the old spelling kept as an alias, with its removal condition stated?
- Is there a **Breaking:** CHANGELOG entry when the policy above applies?
- Do consumer tests cover the old and new spellings?
- Are the affected downstream issues linked?
- Are published tags, assets and historical records untouched?

The gate is a review requirement. Automating it belongs with the first change
that implements a rename.
