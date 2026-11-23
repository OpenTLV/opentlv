# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
