# Security Policy

OpenTLV parses Tag-Length-Value data that may come from untrusted sources
(binary protocol messages, card/device data, files). Vulnerability classes of
particular interest include out-of-bounds reads or writes, integer overflows,
malformed or non-canonical length handling, excessive resource consumption or
denial-of-service conditions, parser state corruption, undefined behavior, and
other memory-safety issues.

## Supported versions

OpenTLV is pre-1.0 (currently `0.x`) and does not yet maintain parallel
maintenance branches. Security fixes are applied to the latest release on the
`main` branch; only the most recent `0.x.y` release receives security updates.
Once 1.0.0 ships, this section will be updated to describe the supported
major/minor lines.

| Version | Supported |
| --- | --- |
| Latest `0.x.y` release | :white_check_mark: |
| Older `0.x.y` releases | :x: |

## Reporting a vulnerability

Report privately through [Core's vulnerability form](https://github.com/OpenTLV/opentlv/security/advisories/new).
Do not disclose suspected vulnerabilities in public issues, discussions or pull requests.
Follow the [organization security policy](https://github.com/OpenTLV/.github/blob/main/SECURITY.md)
for report contents, fallback contact paths, triage and coordinated disclosure.
This local entry point retains Core's supported-version policy; it does not extend
support for standalone bindings or tools, which own their dependency updates and releases.
