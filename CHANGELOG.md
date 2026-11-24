# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Added checks for test artifacts and test dependencies in relocated static
  and shared production packages consumed by C99 and C++11 projects. (#27)
- Added Fixed TLV integration tests with independent wire vectors, seeded
  binary round trips and truncated streams across all 16 length layouts. (#26)
- Added an IO GoogleTest suite for one-shot and cursor reads and writes.
  It checks borrowed views, diagnostic offsets and counts, and preservation
  of committed bytes when an append fails. (#25)
- Added GoogleTest coverage for the public Core API, including wire bytes
  for length fields of 1–8 bytes in both byte orders, borrowed values,
  size limits on 32-bit and 64-bit hosts, and result mappings. (#24)
- Added an opt-in Core smoke test discoverable through CTest. Test builds
  accept a checksum-verified GoogleTest archive for offline use. (#23)
- Added runnable C99 writer and C++11 reader examples for the installed
  OpenTLV package, with expected wire bytes and decoded output. (#12)
- Added workflow correctness and security checks for pull requests and `main`.
  Weekly and manual Full CI runs also compile the public headers with Clang
  and test installed static and shared packages with MSVC. (#11)
- Added sanitizer runs and reproducible line and branch coverage reports for
  the fixed TLV unit tests. (#10)
- Added a relocatable CMake package for static and shared builds. C99 and
  C++11 projects can link `OpenTLV::core` and `OpenTLV::io`. (#9)
- Added result categories and messages for Core and IO. Optional reader and
  writer diagnostics report byte offsets and required and available sizes
  for truncated input and short output buffers. (#8)
- Added a bounded fixed TLV writer for caller-provided buffers. A single
  write reports the required size on short output, and failed appends
  preserve the buffer and cursor. (#7)
- Added a bounded fixed TLV reader that returns borrowed elements, advances
  only after a complete element, and distinguishes end of input from truncation. (#6)
- Added borrowed tag, length, value, and element types, fixed-width length
  conversion in either byte order, and checked size calculations. (#5)
- Added the public `<opentlv/tlv.h>` header and documented the C99 and
  C++11 API conventions for the Fixed TLV Foundation. (#4)
- Documented how to contribute, report vulnerabilities privately, and merge
  changes under the repository's branch protections. (#1)
