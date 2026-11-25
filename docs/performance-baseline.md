# Fixed TLV performance baseline

The `callgrind` CI job builds the revision in
[`benchmarks/performance-baseline.txt`](../benchmarks/performance-baseline.txt)
and the candidate on the same Ubuntu 24.04 x86-64 runner. The full Git SHA
identifies the reference source. Rebuilding it for each comparison gives both
revisions the same CPU, compiler, libraries and measurement tools.

| Measurement | Unit and scope | Review rule |
| --- | --- | --- |
| Callgrind `Ir` | Guest instructions per call for 56 C99 and C++17 workloads, measured twice per revision | Both runs must agree. One extra instruction in any workload fails. |
| Footprint | File bytes and bytes per named section for Core, IO, the C99 writer and the C++17 writer, in both static and shared Release builds | One extra byte in any measured file or section fails. A change in the set of sections requires review before the reports can be compared. |
| Google Benchmark | Median wall time in ns per operation for each of 3,584 workloads, from three repetitions of 8,192 iterations | Both revisions must supply valid measurements for the complete catalog. Timing changes are observations with no pass/fail threshold. |

The instruction counter reports whole instructions and the size tool reports
whole bytes. The repeat-count check and the [clean-rebuild size check](testing.md#heap-allocation-and-binary-size)
test reproducibility at that precision. A zero allowance makes each increase
visible to reviewers, even when another workload improves. Elapsed time varies
with CPU frequency, scheduling and background load. Use individual timing
samples and throughput counters in `timing/results.json` to investigate a
workload. The [probe descriptions](testing.md#fixed-tlv-instruction-counts)
explain what each count includes.

## Reproduce a comparison

Start from a clean Git checkout containing the candidate and the baseline commit.
Use Ubuntu 24.04 x86-64 with GCC and G++ 13.3.0 (`gcc-13`, `g++-13`),
Valgrind 3.22.0 including its headers, GNU size, Git, Ninja, CMake 3.21 or
newer, and Python 3.8 or newer. CMake verifies the Google Benchmark 1.9.4
archive. For an offline run, set `OPENTLV_GOOGLE_BENCHMARK_URL` to the
absolute path of that archive.

Run these commands from the repository root in Bash. Keep the two checkouts
on the same host and use fresh build and report directories outside them.

```bash
set -eu
baseline=$(cat benchmarks/performance-baseline.txt)
candidate=$(git rev-parse HEAD)
work=$(mktemp -d)
echo "Performance evidence directory: $work/reports"
git merge-base --is-ancestor "$baseline" "$candidate"
git worktree add --detach "$work/reference" "$baseline"
python3 -B benchmarks/collect_performance.py "$work/reference" "$work/build-baseline" "$work/reports/baseline"
python3 -B benchmarks/collect_performance.py . "$work/build-candidate" "$work/reports/candidate"
git worktree remove "$work/reference"
python3 -B benchmarks/compare_performance.py \
  --baseline "$work/reports/baseline" --baseline-revision "$baseline" \
  --candidate "$work/reports/candidate" --candidate-revision "$candidate" \
  --output "$work/reports"
```

## Read or replay a report

The CI artifact `fixed-tlv-performance` contains `summary.md`,
`comparison.json`, and the `baseline/` and `candidate/` evidence directories.
To verify a downloaded report, check out the workflow's candidate SHA and set
`artifact` to the extracted directory. Replay needs only Python and the
repository's benchmark scripts.

```bash
replay=$(mktemp -d)
python3 -B benchmarks/compare_performance.py \
  --baseline "$artifact/baseline" --baseline-revision "$(cat benchmarks/performance-baseline.txt)" \
  --candidate "$artifact/candidate" --candidate-revision "$(git rev-parse HEAD)" --output "$replay"
cmp "$artifact/comparison.json" "$replay/comparison.json"
```

In comparison schema version 1, `schema_version`, `baseline_revision`,
`candidate_revision` and `status` are present for every outcome.

| Status | Contents of `comparison.json` | Exit code |
| --- | --- | --- |
| `pass` | All deterministic metrics met the baseline. | 0 |
| `regression` | `regressions` lists every metric with a positive delta. The Markdown summary shows the baseline, candidate and increase for each one. | 1 |
| `invalid` | `error` identifies missing, malformed or incomparable evidence. | 1 |

For `pass` and `regression`, each entry in `metrics` and `timing_median_ns`
has `baseline`, `candidate` and `delta` values. Delta is candidate minus
baseline. Look up a count by `Ir/<workload-id>`, a file size by
`footprint/<linkage>/<role>/file_bytes`, or a section by
`footprint/<linkage>/<role>/section_bytes/<section>`. Timing uses the workload ID.
`configuration` records the settings used for comparison, and `probe_sha256`
identifies the two measured Callgrind binaries.

Each evidence directory contains the workload lists, build metadata, the two
Callgrind count tables and raw profiles, static and shared footprint reports,
and individual Google Benchmark timing samples. The collector fingerprints
the adapters, fixtures, probes and measured example programs in
`protocol.sha256`. It records the effective compile and link settings of the
measured CMake targets in `build-settings.json`. The comparator checks those
inputs, the catalog, toolchain and runtime metadata, measurement settings
and artifact roles before calculating deltas.

The comparator accepts one pair of different protocol digests for reference
`8753cc3384cd62533a41ba40dc932f3681da9b4b`. Applying clang-format 18.1.8
with the repository style to the reference's measured C and C++ sources
produces the candidate protocol byte for byte. For this pair, the
`protocol_sha256` object in `comparison.json` records the raw baseline and
candidate hashes. All other settings and the zero-increase thresholds apply.
Any other unequal pair fails comparison.

To verify the formatting transition in a Linux checkout, install the pinned
formatter and run this from the repository root with Python 3.8 or newer.

```bash
PYTHONPATH=benchmarks python3 -B - <<'PY'
import subprocess
from pathlib import Path
from collect_performance import PROTOCOL_FILES

revision = Path("benchmarks/performance-baseline.txt").read_text().strip()
for name in PROTOCOL_FILES:
    reference = subprocess.check_output(["git", "show", f"{revision}:{name}"])
    if Path(name).suffix in (".c", ".h", ".cpp", ".hpp"):
        reference = subprocess.check_output(
            ["clang-format", f"--assume-filename={name}"], input=reference)
    if Path(name).read_bytes() != reference:
        raise SystemExit(f"Protocol differs from formatter output: {name}")
PY
```

## Review a baseline update

Keep the current pin while measuring a proposed change. Include the report
for the current pin in the PR and explain each accepted increase in instructions
or bytes. To adopt the change as the new reference, update the pin in a reviewed
diff to the full SHA of the measured commit and rerun CI. The pin must be an
ancestor of the tested candidate. This procedure also lets reviewers lock in
an improvement as a tighter limit.

For a changed catalog or measurement procedure, include the incompatible
report, identify the affected workloads or artifacts, and collect repeatable
evidence under the new procedure before proposing its pin. Keep both sets
of evidence available for review.

## Hand off release evidence

For v0.1.0, retain the `fixed-tlv-performance` artifact from the successful
**Fixed TLV** Full CI run for the exact release candidate commit. Record the
workflow run URL, artifact identity and both full SHAs with the release
evidence. Verify that `callgrind` passed, `candidate_revision` equals the
candidate SHA, `baseline_revision` equals the pin at that SHA, and `status`
is `pass`. Retain the static and shared `fixed-tlv-resource-usage-*` artifacts
from that run for the allocation and clean-rebuild checks. A missing artifact,
skipped check or mismatched revision leaves the release evidence incomplete.
