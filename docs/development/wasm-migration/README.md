# WASM migration baseline

This is the behavior-preserving extraction baseline for
[Core #697](https://github.com/OpenTLV/opentlv/issues/697), under
[epic #696](https://github.com/OpenTLV/opentlv/issues/696).
Implementation still lives in Core. Source transfer belongs to
[opentlv-js #8](https://github.com/OpenTLV/opentlv-js/issues/8).

## Immutable source identity

- Repository: `https://github.com/OpenTLV/opentlv`.
- Commit: `a9a6f9470c2b285744738b40196bef9d0341fead`.
- Root tree: `d1f63cbc2064a5d19c3dd6f19b97ddab427cfe64`.
- Description: `0.7.1-40-ga9a6f9470`; no tag points at the baseline.
- Nearest tag: `0.7.1`, commit `e12a52ff69a64468eecab084795a0530c273efea`.
- Core version is provenance, not an independent JS package version.

[inventory.json](inventory.json) records exact Git modes, blob IDs, SHA-256,
byte sizes, ownership and proposed migration destinations for the binding,
shared helpers, native library, CMake inputs, Query tests, manuals and workflow
contracts. Hashes describe Git blobs, independent of checkout line endings.
The root tree identifies the complete source tree, including files outside this
focused inventory. Never substitute a branch name for the baseline commit.

```sh
python3 -B scripts/wasm_migration_baseline.py
```

This checks the recorded inventory against immutable Git objects. It does not
claim that a modified working tree has been built. The optional reproduction
runner clones and checks out the exact baseline before building it.

## Ownership and migration map

| Original source | Destination / responsibility |
| --- | --- |
| `bindings/wasm/src/opentlv_wasm.{c,h}` | JS owns the C bridge; initially retain the relative path |
| `bindings/wasm/js/{opentlv,query}.mjs` | JS owns both facades; retain entry points and behavior |
| `bindings/wasm/test/smoke.mjs` | JS owns facade smoke coverage |
| `bindings/wasm/examples/parse.mjs` | JS owns the parse example |
| `bindings/wasm/CMakeLists.txt` | JS owns the module target; standalone integration is #9 |
| `tests/query/wasm_facade.mjs` | JS owns the facade runner; consume fixtures from selected Core |
| `tests/query/corpus.json`, `reference.py`, `facade_fixtures.py` | Core owns shared semantic fixtures and oracle; do not copy them into JS |
| `tests/query/abi.c`, `abi/*.json`, `check_abi.py` | Core owns public Query layouts and ABI verification |
| Other `tests/query/*`, `tlv/*`, `cmake/*`, root `CMakeLists.txt` | Core keeps native semantics, tests and build contracts |
| `bindings/common/{path_query,query_schema}.h` | Core keeps shared native helpers; the bridge consumes these from pinned Core |
| `.github/workflows/wasm.yml` | Preserve commands and tag reproducibility in future JS CI (#10), then retire Core workflow under #698 |
| `.github/workflows/docs-notify.yml` | Coordinate Docs handoff separately; do not remove or transfer blindly |
| `docs/development/webassembly.md` | JS owns the detailed manual after #13; Docs owns assembly and playground UI |
| `docs/concepts/bindings.md` | Core keeps the shared capability contract and existing gap tracking |
| `LICENSE` | Preserve Core MIT attribution on transferred sources; standalone retains its own MIT notice |

Paths are an initial transfer map, not permission to duplicate shared Core
implementation or finalize the later layout issue. The relative includes of
`bindings/common` are dependency requirements for standalone #9.

## Build and generated-output contract

The recorded workflow uses Ubuntu 24.04, Emscripten **4.0.10**, Node **22**, Ninja
and Release CMake configuration. Root CMake requires Emscripten when
`OPENTLV_BUILD_WASM=ON` and forces static Core linkage. The option defaults OFF;
`OPENTLV_BUILD_WASM_TESTS` defaults ON and registers tests only if Node is found.
The workflow's `--no-tests=error` rejects absent test registration.

```sh
emcmake cmake -S . -B build-wasm -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENTLV_BUILD_WASM=ON \
  -DOPENTLV_BUILD_CXX=OFF -DOPENTLV_BUILD_TESTS=OFF \
  -DOPENTLV_BUILD_EXAMPLES=OFF -DOPENTLV_WARNINGS_AS_ERRORS=ON
cmake --build build-wasm --target opentlv-wasm --parallel
ctest --test-dir build-wasm -L wasm --output-on-failure --no-tests=error
```

The target uses `--no-entry`, `MODULARIZE=1`, `EXPORT_ES6=1`,
`EXPORT_NAME=createOpenTLV`, `ENVIRONMENT=web,worker,node`,
`ALLOW_MEMORY_GROWTH=1`, `FILESYSTEM=0`, `ALLOW_TABLE_GROWTH=1`.
Runtime exports are `HEAPU8`, `UTF8ToString`, `stringToUTF8`, `lengthBytesUTF8`,
`addFunction`, `removeFunction`. `EMSCRIPTEN_KEEPALIVE` exports the bridge
declarations recorded in the inventory. No flags or exports change in #697.

`build-wasm/bindings/wasm/dist/` contains generated `opentlv-core.js` and
`opentlv-core.wasm`, plus copied `opentlv.mjs` and `query.mjs`. Serve them together
or preserve the `locateFile` factory override. Generated binaries are build
outputs, not source files to migrate. The original workflow uploads this
directory as `opentlv-wasm`; there is no npm publication step.

The environment flag enables browser, Web Worker and Node loaders. Existing CI
executes Node only; it does not prove browser or worker runtime behavior. The
binding is experimental, has no checked-in TypeScript declarations, and makes
no new browser, bundler, package-name or platform promises here.

## API, ABI and behavior contracts

The entry module exports `FORMATS`, `MODULES`, `hexToBytes`, `loadOpenTLV`.
The factory returns `version`, `formats`, `modules`, `parse` plus
`QueryProgram`, `QueryExecution`, `QuerySchema`, `QueryError`, `Document`, `Node`,
`Format`, `V1Query`, `nameResolver`, `definitionResolver`, `compileQuery`,
`loadQuery`, `querySchema`, `document`. `query.mjs` exports `queryFacade`.
The [original manual](../webassembly.md) documents ownership and methods.

Parse errors are returned in JSON with already parsed elements; invalid JS
arguments throw. Tags/values are uppercase hex, Source ranges are absolute,
and logical Value length excludes trailers. Format availability follows the
compiled Core. Empty input, Fixed configuration, EMV annotations, Bluetooth
padding, BER/DER/CER, LLDP, NFC and memory growth are covered by smoke tests.

Query/Document processing delegates to Core. Preserve explicit close/reset,
dependent-handle lifetimes, copied results and diagnostics, signed 64-bit
integer handling, callback identity and reentry checks, Format/resolver/provider
contracts, Source feeds, retained ordinals, V1 paths, editing and Schema checks.
The existing parse example's introductory comment understates the current API:
Query/Document already exist, although there are no separate Query/write/validate
example files. The source comment is intentionally unchanged in this baseline.

The inventory freezes C bridge signatures. Reproduction also captures built
WASM imports/exports, public facade names and class static/prototype properties.
Release builds minify internal WASM names; the preserved public bridge names
are the Emscripten module's `_opentlv_wasm_*` properties. The runner checks all
57 declared bridge functions there, separately from the raw WASM export list.
The public Query ABI check compiles `tests/query/abi.c` with Emscripten and compares
the observed layout with Core's reviewed architecture snapshot. Export names
alone are not proof of semantic or ABI compatibility.

```sh
python3 tests/query/facade_fixtures.py --output build-wasm/query-fixtures.json
node tests/query/wasm_facade.mjs build-wasm/bindings/wasm/dist build-wasm/query-fixtures.json
emcc -std=c99 -DTLV_STATIC_DEFINE -I tlv/include -I build-wasm/generated/include \
  tests/query/abi.c -o build-wasm/query-abi.js
node build-wasm/query-abi.js > build-wasm/query-abi.json
python3 tests/query/check_abi.py --input build-wasm/query-abi.json
```

## Reproduction and evidence limits

With the pinned toolchain activated and Node 22 first on PATH:

```sh
python3 -B scripts/wasm_migration_baseline.py --run /var/tmp/opentlv-wasm-baseline
```

The output directory must not exist. The runner checks tool versions, clones the
exact baseline, builds two independent directories, runs the smoke test and
example, shared Query facade and ABI checks, snapshots exports, and compares
SHA-256 of all four outputs. Logs, commands, exit codes and log hashes remain in
the output directory; `results.json` is written only after every check passes.

The existing hosted workflow builds twice and compares the four artifact hashes
only for release-tag pushes. Main and relevant PRs build once. Reproducing that
comparison locally at this untagged SHA does not establish a hosted/tagged
release result. Browser/worker execution, standalone compilation, packaging,
Docs publication and release readiness remain separate gates.

### Recorded local result (2026-10-24)

[results.json](results.json) records the successful Linux/WSL run with Emscripten
4.0.10 and Node 22.16.0, exact commands, 18 successful steps, tool output,
capture-script hashes, observed API/ABI and artifact hashes.
[logs.json](logs.json) retains the corresponding logs; UTF-8 encoding each JSON
string reproduces the bytes covered by its `sha256` in the result record.

- CTest: **2/2 passed** (smoke and parse example).
- Shared Query facade: **171 fixtures, 3,422 backend/window checks passed**,
  plus provider, event, edit, Schema, Format, resolver and V1 interoperability.
- Public Query ABI: **32-8** snapshot matched.
- All **57** bridge declarations were present in the public Emscripten module.
- Both independent builds produced identical SHA-256 for all **four outputs**;
  the copied wrappers matched source bytes and the baseline checkout stayed clean.

The source inventory verifier also rejected a deliberately corrupted hash.
Documentation inventory, SPDX, Markdown and pre-commit checks passed locally.
These results describe the exact Core baseline, not the future standalone build.

## Standalone bootstrap and next steps

[OpenTLV/opentlv-js](https://github.com/OpenTLV/opentlv-js) already exists as a
public MIT repository. Its initial `main` commit observed during this task is
`9a651e0da14cfca26c4770bb15e79432904bcba1` (README and LICENSE only). The companion
local change records this baseline in `MIGRATION.md` and a copy of the inventory,
defines ownership and explains that no standalone package is available yet.
It requires its own review and merge; local metadata is not published provenance.

Workspace registration is [workspace #42](https://github.com/OpenTLV/opentlv-workspace/issues/42).
Transfer, standalone build, CI, packaging, layout and documentation remain
opentlv-js #8 through #13. Core removal is #698, after standalone gates pass;
final coordinated verification is opentlv-js #14. No source extraction, workspace
registration, public export change or npm publication is part of this baseline.
