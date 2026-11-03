# Library and Core naming contract

This contract defines what **OpenTLV Library** and **Core** mean, records every
current public identifier with its scope, and sets the compatibility, breaking-change
and migration rules for later renames (#746, epic #702). It renames nothing: every
identifier listed below keeps working unchanged. Implementation belongs to the
issues named in each decision, after the Library v0.9.0 distribution milestone
(#716).

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
| Removed options | `OPENTLV_READER`, `OPENTLV_<FAMILY>` and the other rejected options | Library | Keep the configure-time rejection messages | #747 |
| Documentation install path | `share/doc/OpenTLV` | Library | Keep | #747 |
| Nested CMake project | `project(tlv)` in `cmake/library/`, which also creates `tlv_SOURCE_DIR` and `tlv_BINARY_DIR` cache entries in embedding builds | Build tree | Keep for v0.9.0; the [audit](#cmake-identity-audit) found no build logic that needs it | #747 |

### Modules, headers and API

| Identifier | Current value | Scope | Decision | Owner |
| --- | --- | --- | --- | --- |
| Module targets | `OpenTLV::Core`, `IO`, `Document`, `Query`, `Schema`, `Codec`, `Generator`, `ASN1`, `EMV`, `Bluetooth`, `NFC`, `DHCP`, `LLDP` and integrations | Module | Keep; `OpenTLV::Core` is the Core module only | #747 |
| Aggregate target | `OpenTLV::tlv` (INTERFACE, no archive) | Library | Keep; see below | #747 |
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
| CPack package | `OpenTLV-<version>-<system>-<processor>-<compiler>`, also the top directory inside each binary asset | Library | Keep for v0.9.0; see the [artifact findings](#findings-for-release-artifacts) | #748 |
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
  target, component and asset names are set in #701 and #748.

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
| `fetch-canonical` | `FetchContent_Declare(opentlv ...)`, `FETCHCONTENT_SOURCE_DIR_OPENTLV` | Aliases and embedded footprint; on CMake 3.24+, a later `find_package(OpenTLV)` resolves to the embedded Library |
| `subdirectory` | `add_subdirectory(<checkout> opentlv)` | Aliases and embedded footprint |
| `fetch-package` | `opentlv_core` with `FIND_PACKAGE_ARGS <x.y.z> EXACT CONFIG NAMES OpenTLV` (CMake 3.24+) | Every contract target is imported |

In every mode, each public target from the
[module contract](modules/ownership.json) resolves to the same `OpenTLV::` name:
an alias of `opentlv_<key>` (or `tlv` for the aggregate) from source, an imported
target from the package. The check also pins:

- The package directory holds exactly `OpenTLVConfig.cmake`,
  `OpenTLVConfigVersion.cmake`, `OpenTLVModules.cmake`, `OpenTLVTargets.cmake`
  and one `OpenTLV-<key>Targets.cmake` per component, plus their
  per-configuration files. The component registry matches the module contract.
- `ExactVersion` rejects another patch version, and documentation is installed
  under `share/doc/OpenTLV`.
- Every removed option still fails at configure time with its message.
- An embedding build creates only the `tlv` and `opentlv_<key>` targets. It adds
  only these cache entries: the `OPENTLV_*` options, the `OpenTLV_*` and `tlv_*`
  project variables, and entries owned by CMake's GNUInstallDirs,
  GenerateExportHeader and FetchContent modules.

Unit tests in `scripts/test_module_contracts.py` keep the option list equal to
the `option()` declarations and cover every failure these checks report.

Both FetchContent names work with the current Library. The Library, v0.8.0
included, never reads the dependency name, so the rename itself needs no Library
change.

### Findings for the post-v0.9.0 step

The audit fixed none of these. Each one is a behavior or naming change, which
rule 6 keeps separate, and the sequence above schedules it after v0.9.0.

1. **The build tree looks like a package but is not one.** Configuration writes
   `OpenTLVConfig.cmake` to the top of the build tree, and the Library has no
   `export()`. `OpenTLV_DIR=<build>`, or `CMAKE_PREFIX_PATH=<build>` on Windows,
   finds the file and fails with
   `include could not find requested file: .../OpenTLV-coreTargets.cmake`.
   Either configure the file outside the package search path, or add a build-tree
   export.
2. **FetchContent `FIND_PACKAGE_ARGS` ignores components.** FetchContent calls
   `find_package(<dependency> ... NAMES OpenTLV)`, so requested components arrive
   in `<dependency>_FIND_COMPONENTS`. `OpenTLVConfig.cmake` reads only
   `OpenTLV_FIND_COMPONENTS`. With `opentlv_core` or `opentlv`, every component is
   loaded and an unknown required component is accepted. Reading
   `CMAKE_FIND_PACKAGE_NAME` in the configuration file would fix this.
3. **The legacy name can mix two Libraries.** After
   `FetchContent_MakeAvailable(opentlv_core)` builds from source, a later
   `find_package(OpenTLV)` with an installed Library on the prefix path succeeds.
   It reports the installed `OpenTLV_DIR` and version, while every target stays
   the embedded alias. With `opentlv`, CMake 3.24+ redirects that call to the
   embedded Library, which `fetch-canonical` checks. The redirect exists only when
   the declaration uses `FIND_PACKAGE_ARGS` or `OVERRIDE_FIND_PACKAGE`. This
   supports the canonical-name decision.
4. **A redirect carries no version.** CMake's generated redirect accepts any
   requested version and leaves `OpenTLV_VERSION` empty. The redirect includes an
   optional `opentlv-extra.cmake`; evaluate whether the Library should provide it.
5. **Unnamespaced names in embedding builds.** The aggregate's build target is
   named `tlv`, the name of the removed monolithic library. The nested
   `project(tlv)` adds `tlv_SOURCE_DIR`, `tlv_BINARY_DIR` and `tlv_IS_TOP_LEVEL`
   to the application's cache. Nothing in the build reads them: the module
   helpers use `OpenTLV_SOURCE_DIR` and `OpenTLV_BINARY_DIR`. An application
   that already has a target named `tlv` fails to configure. One candidate is a
   build target `opentlv_tlv` with `EXPORT_NAME tlv`, which keeps `OpenTLV::tlv`
   and `OpenTLVTargets.cmake`, together with removing the nested project. Both
   identities are inventoried, so the breaking-change policy applies.

## Release artifact audit

The first part of #748 audits the release identities against the published
Library v0.8.0 and the current `core-release.yml`, before v0.9.0. It renames
nothing. `scripts/test_core_release.py` and `scripts/test_ci_workflows.py` now
fail when any identity in the contract below changes.

### Artifact contract

This is version 1 of the artifact contract. It describes v0.8.0, the first
release published by `core-release.yml`, and every later release until a change
under the [breaking-change policy](#breaking-change-policy). `X.Y.Z` is the
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

The audit fixed none of these except the release title above. Each one changes
a published identity or verifier-visible evidence, so it follows v0.9.0 or the
binding issues named.

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
   their verifiers before moving their pins. For the Library,
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
6. **Separate changes.** A naming change contains no behavior, API, ABI or
   build-option change, and a behavior change contains no rename.

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
#746 contract -> v0.9.0 (#716) -> #747 CMake -> #748 artifacts
  -> Library release with aliases -> bindings and tools
  -> Workspace -> documentation -> alias removal
```

1. Approve this contract (#746). No identifier changes.
2. Release Library v0.9.0 (#716) with the current identifiers. Naming work
   does not block package publication.
3. Implement #747, then #748, in the Library, adding aliases only.
4. Publish a Library release containing the new names and their aliases.
5. Migrate each binding and tool separately, pinned to that release, through its
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
