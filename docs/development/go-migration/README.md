# Go migration baseline

This is the pre-extraction record for [Core #685](https://github.com/OpenTLV/opentlv/issues/685)
under [epic #684](https://github.com/OpenTLV/opentlv/issues/684).
It freezes existing behavior for comparison during migration. The implementation
remains in Core until the separately tracked extraction and cleanup work.

## Source and module identity

| Item | Baseline |
| --- | --- |
| Core repository | `https://github.com/OpenTLV/opentlv` |
| Exact Core commit | `cb3caed5b09706ce11ecfae839397fd4f99d0025` |
| Source subtree | `bindings/go` |
| Module path | `github.com/marekcingel/OpenTLV/bindings/go` |
| Public package | `opentlv` |
| Go language directive | `go 1.22` |
| Third-party Go dependencies | None; no `go.sum` |
| Independent binding release | None recorded; no `bindings/go/*` tags reachable from the baseline |
| Existing `Version()` | Reports the linked **Core** version, not a Go binding release |
| License | MIT; original sources and Core LICENSE attribute Marek Cingel, 2026 |

The nested module's documented release tag prefix is `bindings/go/`. Canonical
standalone module identity and independent versioning belong to
[Go #4](https://github.com/OpenTLV/opentlv-go/issues/4). This baseline does not
rewrite imports, assign a binding version or publish a module.

## Implementation and ownership inventory

[inventory.json](inventory.json) records exact Git blob IDs, SHA-256 digests and
byte lengths from the pinned commit, independent of checkout line endings. It
contains every file in `bindings/go`, both Go guides, and the listed shared
inputs and integration references. Its hashes describe the **original source
paths**, not a future standalone layout.

| Area | Source files and migration ownership |
| --- | --- |
| Public facade | `opentlv.go`, `format.go`, `element.go`, `reader.go`, `writer.go`, `document.go`, `errors.go`, `diagnostic_kind.go`, `codec.go`, `query.go`, `query_v1.go`, `program.go`, `query_extensions.go`; Go-owned |
| Native bridge | All `internal/capi/*.go`, `bridge.c`, `bridge.h`; Go owns cgo, callbacks, unsafe conversions and native lifetime adaptation |
| Tests | 14 `tests/*_test.go` consumer/architecture/diagnostic tests, `element_test.go`, `internal/capi/bridge_test.go`; Go-owned |
| Runnable examples | `version`, `quick_start`, `reader`, `writer`, `errors`, `codec`, `document`, `query`, `query_conformance`; Go-owned |
| Module and documentation | `go.mod`, binding README, `docs/guides/go.md`, `docs/development/go.md`; Go-owned |
| Existing CI | `.github/workflows/go.yml`; replacement belongs to Go #3; Core removal is deferred |
| Generated diagnostic vocabulary | Go owns generated `diagnostic_kind.go`, `internal/capi/diagnostic_strings.go` and `tests/diagnostic_vocabulary_test.go`; Core owns canonical C enums/name tables and `scripts/generate_diagnostic_vocabulary.py` |
| Shared Query corpus | `tests/query/corpus.json`, Python runner/oracle and supporting inputs remain Core-owned; the Go adapter consumes the pinned corpus |
| Cross-language documentation | Core keeps conceptual guides, example catalogs, documentation inventories and native reference ownership; Go-specific prose/examples are handed off through Go #6 |
| Native library | Core owns C implementation, public/generated headers, CMake exports and comprehensive native tests; these are not Go sources to copy |

The machine-readable inventory includes Core files that reference the nested
module (excluding historical changelog entries). These entries identify later
integration cleanup, not authorization to move shared Core documentation or tests.

## Configuration and native linking contract

Go 1.22+, `CGO_ENABLED=1`, a compatible C compiler, and matching Core headers and
library are required. The bridge links `-ltlv`; it has no pure-Go fallback,
automatic Core download or dependency pin at this baseline. Callers provide
source/public and generated include paths in `CGO_CPPFLAGS` and the library
directory in `CGO_LDFLAGS`. Generated `tlv/config.h` must match the linked binary.
The bridge explicitly requires Reader, Writer, Query, Schema and Codec.

| Configuration | Existing contract | Baseline coverage |
| --- | --- | --- |
| Linux amd64, default, static | Existing CI configuration; `TLV_STATIC_DEFINE` required | Local reproduction |
| Linux amd64, minimal, static | Existing CI; ASN.1, Bluetooth, LLDP, DHCP, NFC and Document disabled | Local reproduction; feature-dependent tests may skip |
| Linux amd64, default, shared | Documented prebuilt-library mode; omit `TLV_STATIC_DEFINE` and set runtime library search path | Additional local reproduction |
| Windows, GCC/MinGW | Documented static setup; Go target, compiler and library architecture must agree; 32-bit MinGW requires `GOARCH=386` | Not executed for this baseline |
| Windows shared | Documented matching library/runtime `PATH` setup | Not executed for this baseline |
| macOS or other targets | No matrix evidence supplied by the existing Go workflow | Not validated by this record |

Minimal builds keep generic Fixed Reader/Writer, error and codec operations.
Document and Query document examples are omitted there. Unavailable presets
return unsupported status; no protocol fallback is introduced. Race testing is
recorded on Linux amd64; the documented Windows 386 setup cannot run it.
External native changes require a fresh Go cache or `go clean -cache`.

## Ownership and lifetime contracts to preserve

- `internal/capi` is the exclusive boundary for `C`, `unsafe`, C handles and
  pointer conversions. Consumer programs import the public Go package.
- Synchronous byte arguments are borrowed only for the native call. C does not
  retain Go pointers. `runtime.KeepAlive` protects projection after native calls;
  nil and non-nil empty slices retain their documented distinctions.
- Immutable Formats may be shared. Reader/Writer descriptors and Fixed contexts
  are call-local; Readers/Writers own Go state and require no native `Close`.
  Parsed Values/source-bound Tags borrow the input; `Clone()` creates a snapshot.
  Format-generated Tags and diagnostics are copied into Go storage.
- Writers borrow output buffers, reject unsafe overlap, report required
  capacity and preserve retry behavior. Tree Writer parents stage Go buffers.
- Document copies input and owns C allocations/Format context. Explicit `Close`
  is deterministic, idempotent and nil-safe; finalization is a fallback. Copies
  share closure state. Nodes retain their owner; successful edits invalidate
  existing handles. Node Tags/Values and diagnostic results are owned snapshots.
- Query programs own reference-counted native images. Existing executions retain
  their native reference after program closure. Executions own C workspaces and
  copied input/feed storage, and require deterministic cleanup. Callback reentry
  restrictions, panic conversion, provider concurrency and `runtime/cgo.Handle`
  release behavior must survive migration. V1 matchers also own closable storage.
- Codecs delegate to Core and return owned values. Error status identity,
  `errors.Is`/`errors.As`, diagnostic presence flags, byte offsets and paths remain
  unchanged. There is no new public Schema facade in this migration step.

See the pinned binding README and the corresponding test files in the inventory
for detailed contracts. This record does not strengthen thread-safety guarantees.

## Reproduce and verify

Verify the inventory from any Core checkout that contains the pinned Git objects:

```sh
python3 -B scripts/go_migration_baseline.py
```

On Linux with Go, GCC, CMake, Ninja, Git and Python 3 available:

```sh
python3 -B scripts/go_migration_baseline.py --run /var/tmp/opentlv-go-685-baseline
```

The output directory must not exist. The runner verifies the inventory, makes
a detached local clone at the exact source commit, and builds Release C libraries
with C++/native tests/examples disabled. It uses separate fresh Go caches for
each configuration. It runs formatting, vet, uncached package tests, race tests,
`GOEXPERIMENT=cgocheck2`, all applicable examples and the shared Query corpus
with `--property-cases 0`. It verifies that tracked source files remain unchanged.
It writes command logs and `results.json`, including tool versions, cgo flags,
exit codes, test/skip counts and log hashes. A failure stops the run and is saved.

The committed `results.json` is a local observation, not hosted CI or release
evidence. Raw logs stay in the selected output directory; their hashes allow
comparison with the record but do not make those local logs remotely available.
Go test event counts include subtests. Explicit skips are retained in the record.
New runs may differ in timing, host details, temporary paths and log hashes.

### Recorded local results

The run used Ubuntu 24.04.1 under WSL, Linux amd64, Go 1.22.2, GCC 13.3.0,
CMake 3.28.3 and Ninja. The linked Core reported `0.7.1`; source identity remains
the full commit above. This is not a Go binding version.

| Native configuration | Regular tests | Race tests | cgocheck2 | Runnable examples | Shared Query corpus |
| --- | --- | --- | --- | --- | --- |
| Default static | 66 passed | 66 passed | 66 passed | 8 passed | 171 fixtures, 2,133 native executions |
| Minimal static | 40 passed, 26 skipped | 40 passed, 26 skipped | 40 passed, 26 skipped | 6 passed | Omitted with Document disabled |
| Default shared | 66 passed | 66 passed | 66 passed | 8 passed | 171 fixtures, 2,133 native executions |

Formatting and vet passed for all three configurations. The
minimal skips reflect disabled optional features; they are not full-feature
coverage. The shared Query run includes zero generated property comparisons,
matching the existing Go CI invocation. Windows, macOS, hosted CI, independent
standalone builds and release publication were not run for this issue.

The [recorded results](results.json) contain the individual checks. Raw logs were
also copied to the local Core `build/issue685-evidence/` directory. The original
tracked sources remained byte-for-byte unchanged throughout validation.

## Standalone repository handoff

`https://github.com/OpenTLV/opentlv-go` already exists as a public repository
with default branch `main`, description "Go bindings for OpenTLV — a universal
library for parsing, encoding, and working with TLV and TLV-like binary formats."
and MIT license metadata. Its initial commit is
`cb63f4e549f22e4d8827acc5b53b09ef4aa8cd8c`, containing README and LICENSE.
Metadata and source were inspected on 2026-10-22. Its initial LICENSE credits
OpenTLV, 2026; the prepared attribution addition retains the original Marek Cingel
copyright alongside it. Source SPDX notices must remain intact on import.

The companion local branch `feature/685_go_migration_baseline` in `opentlv-go`
adds `MIGRATION.md`, repository scope/entry-point guidance and license attribution.
This establishes provenance before source transfer; it does not claim that the
standalone repository already builds. Changes are prepared locally for review.

Follow-up ownership: Workspace #40 integrates the checkout; Go #1 moves sources,
tests and examples; Go #2 establishes verified Core integration; Go #3 owns CI;
Go #4 owns module/version metadata; Go #5 normalizes layout; Go #6 integrates
documentation; Core #686 removes migrated implementation; Go #7 verifies the
completed migration. Full epic/release readiness remains outside #685.
