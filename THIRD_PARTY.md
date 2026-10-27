# Licensing and third-party attribution

Original OpenTLV Core code and documentation are licensed under MIT; see
[LICENSE](LICENSE) and the per-file notices or [REUSE.toml](REUSE.toml).
Marek Cingel retains copyright in his original contributions. Other contributors
retain their own copyright and years. OpenTLV is a GitHub organization, not a
separate legal copyright holder; contributing does not transfer ownership.

## Dependencies and generated files

The current source tree does not vendor GoogleTest or Google Benchmark. Optional
test and benchmark builds fetch them into the build directory; their upstream
copyright and license files remain authoritative. These dependencies are not
installed in Core's native packages. Preserve their notices when redistributing
them separately or adding a dependency to a package.

CMake generates package support files and the export header using its own
templates. Preserve the notices supplied by CMake. Core's configuration and
version templates carry MIT headers which their generated C/C++ outputs inherit.
Build outputs and downloaded dependencies are ignored by Git and are outside the
source-tree REUSE scan. Tracked files under `vendor/`, `third_party/` or
`generated/` are exempt only from Core's header-layout check, not from REUSE.
They must retain accurate upstream metadata and corresponding license texts.

Binary examples and fuzz corpora are Core fixtures. Their metadata is listed by
exact path in `REUSE.toml`; do not change payload bytes to add notices. New imported
fixtures need an ownership review and their original licensing, not automatic
attribution to Marek Cingel.

## Community policies

The [organization Code of Conduct](https://github.com/OpenTLV/.github/blob/main/CODE_OF_CONDUCT.md)
is an adaptation of Contributor Covenant 2.1, licensed under CC BY 4.0 with its
upstream attribution preserved in `OpenTLV/.github`. Core links to that policy
and does not include a copy in its source or native packages.

## Distribution

Source release archives include `LICENSE`, `LICENSES/`, `REUSE.toml`, this notice
and source headers. The release-generated `opentlv-source.txt` identity file is
covered by an explicit REUSE annotation. Native installation and CPack archives
include `LICENSE`, `LICENSES/` and this notice under `share/doc/OpenTLV` (or the
configured CMake data directory), alongside notices in installed headers.
Repository-relative links in this notice describe the source distribution;
native packages contain only the installed subset. Keep all applicable notices
when redistributing that subset. Shared GitHub defaults are not copied into
clones or release packages and do not replace local licensing files.
