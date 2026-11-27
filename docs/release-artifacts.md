# Fixed TLV release artifacts

## Artifact matrix for v0.1.0

| Distribution | Filename | Contents | Target |
| --- | --- | --- | --- |
| Source | `opentlv-0.1.0-source.tar.gz` | Fixed TLV Core and IO source, public C99 and C++17 headers, CMake project, examples, tests, documentation and MIT license | One archive for all [tested compiler and architecture combinations](testing.md#compiler-and-architecture-matrix) |

v0.1.0 ships as source only. Build static or shared libraries using CMake
3.15 or newer and a C99 compiler. The default build works offline. Extract
the archive, change to `opentlv-0.1.0/` and follow the
[build and install instructions](../README.md#build-and-install).

The archive contains the commit's tracked files, including `include/opentlv/`,
`src/core/`, `src/io/`, the root `LICENSE` with its copyright notice, and
the root `CMakeLists.txt` declaring the package version. It also includes
the changelog, benchmarks and contributor tooling. The installed CMake
package exports `OpenTLV::core` and `OpenTLV::io`.

## Build the archive

In a clean checkout of the candidate, use Git and Python 3.8 or newer to package
the reviewed commit. The following Bash commands build the stable archive,
list its contents and print the commit ID stored in its tar header.

```bash
candidate=$(git rev-parse HEAD)
output=build/release/dist
python3 -B scripts/package_source.py --ref "$candidate" --output-dir "$output"
archive="$output/opentlv-0.1.0-source.tar.gz"
tar -tzf "$archive"
gzip -dc "$archive" | git get-tar-commit-id
```

The default version comes from the candidate's CMake project. For a
prerelease, add `--version 0.1.0-rc.1` to produce
`opentlv-0.1.0-rc.1-source.tar.gz` with the root `opentlv-0.1.0-rc.1/`.
The numeric version must match CMake. The filename and root preserve any
SemVer prerelease or build identifiers. Compare the printed commit ID with
the full candidate SHA recorded for the [Full CI run](quality.md#full-ci-and-release-evidence).

## Verify the archive

Use the release check on the same candidate with Git, Python, CMake, CTest,
C99 and C++17 compilers, and the symbol inspection tools used by the
[package checks](architecture.md#verification).

```bash
python3 -B tests/check_release_artifact.py build/release/verified --ref "$candidate"
```

The check builds the archive in two clean checkouts, compares their bytes
and checks the archive name, root, commit metadata, file inventory and
contents against the candidate, including the license. It extracts the source
and runs the static and shared package checks with symbol inspection. Those
checks build and run C99 and C++17 consumers and the documented examples.
On success, the check prints the tarball's SHA-256 digest and leaves the file
in `build/release/verified/`. Pass the same `--version` to both commands
when verifying a prerelease. Full CI uploads the tarball in the Actions
artifact `fixed-tlv-source`. The [Release workflow](governance.md#release-automation)
supplies the full requested version. Direct Fixed TLV runs use the default
CMake version.

Git supplies the committed file bytes, executable bits and commit timestamp.
Use clean checkouts with no local `.git/info/attributes` overrides. The
gzip header uses time zero and omits the output filename. With the same
commit, release version and packaging toolchain, clean checkouts at different
paths produce identical archive bytes. Git, Python or zlib version changes
can affect the compressed bytes. The reproducibility check covers the source
tarball. Compiled library bytes depend on the consumer's build toolchain.
