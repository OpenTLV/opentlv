# Rust binding development

The `opentlv-sys` and `opentlv` crates, their tests, examples, packaging and CI
are maintained in [OpenTLV/opentlv-rust](https://github.com/OpenTLV/opentlv-rust).
Core no longer contains the `bindings/rust/` Cargo workspace or a Rust workflow.

## Build

Follow the standalone [development guide](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/development/rust.md#build)
and the [user guide](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/guides/rust.md)
for setup, Cargo features and examples. For local Core development, set
`OPENTLV_CORE_SOURCE` to the absolute Core checkout path, as described in
[local workspace development](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/development/rust.md#local-workspace-development).
The build script consumes the public C API and selects Core components for its
Cargo features; it owns its linking policy.

Core still owns the inputs that the standalone Rust tests read from the selected
Core tree: the shared `tests/query/corpus.json` and `tests/query/run.py`, which
drive the `query_conformance` example, and the fuzz seed corpora under
`tests/fuzz/`, which `opentlv/tests/corpus.rs` replays through the safe API.
Keep those paths stable or coordinate changes with that repository.

## Versioning

Crate versions follow the independent binding policy and are not derived from
Core tags. `opentlv::version()` reports the linked Core library. See the
standalone [versioning section](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/development/rust.md#versioning).

## Diagnostic vocabulary

Core owns `scripts/generate_diagnostic_vocabulary.py` because the canonical
enums and their names come from its public headers and C name tables. Its
default invocation and `--check` cover only the in-tree Go outputs. To refresh
or check the Rust outputs against this Core checkout, explicitly select the
standalone checkout from the Core repository root:

```sh
python -B scripts/generate_diagnostic_vocabulary.py --rust-output-root ../opentlv-rust
python -B scripts/generate_diagnostic_vocabulary.py --check --rust-output-root ../opentlv-rust
```

The destination must already contain the `opentlv/src` and `opentlv/tests`
directories and must be outside the Core source tree. Generation does not
select a new Core dependency for Rust. Review any vocabulary change together
with that repository's Core dependency update and run its tests.

## Continuous integration

The standalone [quality gates](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/quality-gates.md)
own Rust PR/Main/Full validation. Core PR/Full workflows continue to own native
validation and shared fixtures; they do not build or test the Rust crates.
Cross-language Query release evidence still requires Rust results from its
owning repository.

## Migration

The [migration record](https://github.com/OpenTLV/opentlv-rust/blob/main/docs/migration.md)
freezes Core commit `476f1fbecde6b056419e43c9a6cea4d1ca8df1cf`, including all
66 original `bindings/rust/` files, the Rust workflow and both Rust guides.
Their history remains in Core. The workspace supplies `opentlv-rust` as a
sibling checkout, and the portal consumes the standalone Rust guides.
