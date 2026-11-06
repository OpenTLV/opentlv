# Amalgamation analyzer triage

This contributor audit records the correctness review for [#764](https://github.com/OpenTLV/opentlv/issues/764),
using baseline `109b5d114244b10a34cbd3be2561e3096dcebb40` and the modular sources
at `f1a6e5da`. See the [amalgamation contract](amalgamation.md) for distribution
equivalence and the [quality gates](quality-gates.md) for release requirements.

The reproduced paths below do not demonstrate a reachable correctness defect.
This change adds regression coverage and records the reasons for the false
positives. It does not initialize outputs merely to silence the analyzer,
suppress additional warnings, modify generated code, or add an analyzer CI gate.
The disposition applies to these paths and the documented API contracts, not
to all future warnings.

## Reproduction

On Ubuntu 24.04 x86-64, Clang `18.1.3 (1ubuntu1)` reports 12 warnings from the
baseline amalgamation. The issue did not include its original compiler version
or command; the following is the independently reproduced configuration.
The archived checkout has no Git metadata, so the generated banner reports
version `0.0.0` and commit `unknown`; its C inputs are the exact baseline.

```sh
git archive --format=tar --output=baseline.tar 109b5d114244b10a34cbd3be2561e3096dcebb40
mkdir baseline
tar -xf baseline.tar -C baseline
cmake -S baseline -B baseline-build -G Ninja \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DOPENTLV_BUILD_CXX=OFF -DOPENTLV_BUILD_TESTS=OFF \
  -DOPENTLV_BUILD_EXAMPLES=OFF
cmake --build baseline-build --target opentlv_amalgamation
clang-18 --analyze -std=c99 -Xanalyzer -analyzer-output=text \
  baseline-build/amalgamation/opentlv.c 2> baseline-clang.log
```

For standalone comparisons, replace the compiler in each matching entry of
`baseline-build/compile_commands.json` with `clang-18 --analyze -Xanalyzer
-analyzer-output=text`, removing `-c` and `-o <object>`. Retain the generated
include path, module export definition, `-fPIC`, `-fvisibility=hidden` and
`-Werror`. The checked translation units are `core/formats/packed.c`,
`builtin/emv/emv_codec.c`, `builtin/asn1/der_validation.c`, `document/writer.c`,
`document/reader.c`, `query/evaluation.c`, `schema/report.c`,
`schema/structure.c` and `io/writer/tree.c`, all beneath `src/`.

Clang's analyzer exits successfully even when it reports warnings. Exit code
zero is therefore not evidence of a warning-free analysis. Raw local commands
and traces are retained in `build/issue764/`; the table identifies modular
locations and line numbers in the reproduced baseline amalgamation.

Clang 21.1.3 on Windows (`x86_64-pc-windows-msvc`) reproduced the same 12
warnings with the same flags against a copy of these generated files. An
additional GCC 13.3.0 `-std=c99 -O0 -Wall -Wextra -fanalyzer -c opentlv.c`
attempt did not complete before the WSL environment restarted; it had reached
about 11 GB resident memory. That incomplete attempt is not a successful GCC
analyzer check. GCC validation below uses the normal Release build and tests.

## Diagnostic dispositions

| Baseline amalgamation lines | Modular location and diagnostic | Disposition and path evidence |
| --- | --- | --- |
| 1551, 1556, 1561 | `core/formats/packed.c`, `tlv_packed_decode`: undefined `count` or `type` | **False positive (3 warnings).** The trace takes `size < field->storage_size` after successful layout validation and `size >= header_size`. Validation requires both field storage sizes to equal the nonzero header size and validates each field by reading an eight-byte zero buffer. These conditions cannot coexist. Null input with zero size exits at header truncation; null input with nonzero size exits at argument validation. The ignored reads succeed for this validated immutable layout. |
| 4077 | `document/reader.c`, `tlv_document_parse`: potential leak of `created` | **False positive.** The trace follows the default `malloc` allocator and parse failure, but omits the release by `tlv_document_free(created)` immediately before return. The document is private during parsing, starts with zero active callbacks, and cannot enter deferred destruction. `discard_children` frees partial nodes and Values; `tlv_document_memory_release` invokes the copied allocator's release callback (`free` on this path). |
| 4102 | `document/reader.c`, `tlv_document_builder_create`: potential leak of `document` | **False positive.** Builder allocation failure calls `tlv_document_free(document)` immediately before returning. After successful creation, cancellation and consume failure also free the document; successful consumption transfers it to the caller. Builder destruction uses its own copied allocator after document destruction or transfer. |
| 4280 | `document/writer.c`, `encode_as`: undefined `total` | **False positive.** The trace takes an event callback error in `tlv_tree_writer_measure_events_observed`, returns through `tree_error` without assigning size, then assumes the returned status is `TLV_OK`. `tree_error` returns its input status unchanged. Every successful measurement assigns `*size` after checking balanced events; `prepare_encoding` only retries its own insufficient-workspace result. Thus `capacity < total` is reached only with an assigned total. |
| 6739 | `query/evaluation.c`, `scalar_call`: null `hook` | **False positive.** `compatible` rejects a missing hook, missing decoder, wrong function or insufficient scratch before `tlv_query_eval_init` publishes an execution. The program and environment, including the borrowed hook array, must remain immutable during execution. An execution that passes initialization has the matching hook; changing it afterward violates the lifetime contract. |
| 10673 | `schema/report.c`, `validate_all`: null `current` | **False positive.** The trace chooses an arbitrary `item.depth` on the first successful Tree Reader call, indexing an unset schema frame. The Reader starts at depth zero, whose frame is set to the checked root schema. Every descent into modelled children stores the next frame first. Unmodelled or mismatched constructed subtrees are skipped. A checked schema cannot have a null root. |
| 15673 | `builtin/asn1/der_schema.c`, `read_input`: undefined `root_used` | **False positive.** The trace enters the non-OK tag-read branch of `tlv_der_read_element`, then assumes `der_fail` returned `TLV_OK`. `der_fail` returns the supplied error unchanged. `read_one` and `read_input` both propagate failure before using `root_used`; every successful header read assigns `*consumed`. |
| 19192, 19205, 19219 | `builtin/emv/emv_codec.c`, `tlv_emv_value_decode`: undefined number in date, time or account | **False positive (3 warnings).** The trace takes an invalid BCD nibble in `tlv_number_decode`, then assumes `tlv_codec_diagnostic_reason_at(..., TLV_ERR_INVALID_VALUE, ...)` returns `TLV_OK`. The helpers preserve the supplied result. `read_number` succeeds only after the numeric decoder copies its initialized number into the output. Invalid BCD returns before domain conversion or publication. |

### Standalone and shared-header comparison

Standalone Clang 18 reproduces the Query hook and Schema report warnings above,
plus two warnings absent from that compiler's amalgamation run:

| Modular location | Disposition and path evidence |
| --- | --- |
| `io/writer/tree.c:229`, `measure_close`: null `frame` | **False positive.** Measurement initializes depth to zero and rejects an END at depth zero. Only successful BEGIN increases depth, after checking frame capacity; initialization rejects null frames with nonzero capacity. A close cannot reach `frames[depth - 1]` without a published frame. |
| `schema/structure.c:36`, `violation`: uninitialized `path[i]` | **False positive.** The standalone trace treats the Reader's successful output depth as unconstrained. Root violations have depth zero and never enter the path loop. Before descending into each modelled child, `validate_input` stores `path[item.depth + 1]`. Unmodelled descendants are skipped, so every path entry read by `violation` has been initialized. |

The issue also names `schema/internal/traversal_internal.h`. This Clang 18
command emits no warning directly at that header; both including units
(`report.c` and `structure.c`) were analyzed. Reader failure detail is supplied
only when `diagnostic` is non-null, and the subsequent access is guarded by
`diagnostic && rc != TLV_OK`. Its stack frames are supplied to a successfully
initialized bounded Tree Reader. The header and the depth warnings above are
one shared traversal review, not a reproduced null dereference in the header.

## Regression coverage

The same tests run against conventional, relocated-installed and amalgamated C
backends through `scripts/check_distribution_equivalence.py`:

- `Unit_Tlv_Packed.InvalidFieldsNeverPublishDecodedValues` rejects malformed tag
  and length configurations without publishing output. Existing truncation,
  scope-underflow and header-width tests cover validated layouts.
- `Unit_Tlv_Emv.InvalidBcdNeverPublishesDateTimeOrAccount` corrupts each high and
  low nibble and checks output preservation with and without diagnostics.
- `Unit_Tlv_DerSchema.MalformedRootHeaderNeverPublishesOutputs` covers malformed
  identifiers, missing/truncated lengths and truncated Values through a checked
  schema, preserving caller outputs with and without diagnostics.
- `Unit_Tlv_Document.MalformedNestedInputReleasesPartialTreeWithCallerAllocator`
  combines malformed nested input with failure at every preceding allocation,
  both with and without retained source locations.
- `Unit_Tlv_DocumentBuilder.MalformedInputReleasesPartialTreeBeforeBuilderDestruction`
  verifies immediate partial-document cleanup on consume failure, leaving only
  the builder allocation until destruction. Existing allocator sweeps cover
  builder allocation failure and cancellation.
- `Unit_Tlv_TreeWriter.EventMeasurementRejectsUnmatchedCloseWithoutFrames`
  rejects END on an empty stack with no frame storage and unchanged size output.
- Existing Document callback-failure tests preserve sizes and destinations on
  measurement failure. Query compatibility tests reject missing hooks before
  execution. Schema tests exercise deep diagnostic paths, unknown constructed
  subtrees and optional diagnostics.

## Local validation and release evidence

Local results on 2026-11-06, with the six new regressions:

| Configuration | Result |
| --- | --- |
| GCC 13.3.0 Release, static libraries | 148/148 CTest entries passed, with no skipped tests. |
| Clang 18.1.3 Debug, ASan/UBSan, conventional | 68/68 CTest entries, including 1,274 GoogleTest cases; all 14 fuzz targets replayed successfully. |
| Same Clang configuration, relocated-installed | Same test inventory and observable results; all checks passed. |
| Same Clang configuration, amalgamation | Same test inventory and observable results; all checks passed, including symbol equivalence. |

Module contracts, documentation inventories (including 11 inventory tests),
and repository pre-commit checks passed. The local summary and source hashes
are in `build/issue764/validation.json`, with the complete distribution report
and test logs in `build/issue764/clang-final/`.

Build and test GCC Release, then run the existing equivalence gate with Clang
Debug sanitizers:

```sh
cmake -S . -B build/issue764-gcc -G Ninja \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release -DOPENTLV_BUILD_SHARED_LIBS=OFF \
  -DOPENTLV_BUILD_PROPERTY_TESTS=ON
cmake --build build/issue764-gcc --parallel 6
ctest --test-dir build/issue764-gcc --output-on-failure --parallel 6
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  python -B scripts/check_distribution_equivalence.py \
  --output build/issue764-clang --cc clang-18 --cxx clang++-18 \
  --config Debug --sanitize --jobs 4
```

An existing GoogleTest checkout can be supplied with `--gtest-source <directory>`.
The gate checks common test inventories, observable results, symbol inventories
and fuzz corpus replay across all three distribution forms. These are local
checks. Hosted Full CI for the final published commit remains a separate
acceptance requirement; this audit is not release approval.
