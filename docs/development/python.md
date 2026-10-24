# Python binding development

The Python extension, facade, tests, examples, packaging and CI are maintained
in [OpenTLV/opentlv-python](https://github.com/OpenTLV/opentlv-python).
Core no longer provides `OPENTLV_BUILD_PYTHON` or builds Python packages.

## Build

Follow the standalone [build and dependency guide](https://github.com/OpenTLV/opentlv-python/blob/main/docs/development/python.md).
For local Core development, build the native distribution from that repository
with `OPENTLV_PYTHON_CORE_SOURCE` set to the absolute Core checkout path, then
install its public facade. The extension consumes the public `OpenTLV::tlv`
target and validates its required features; it owns its static-linking policy.

Core still uses the Python interpreter for native Query/property tests,
documentation checks and build/release tooling. Removing the binding does not
remove those tools or the shared `tests/query/corpus.json` and `reference.py`.
Native binding adapters are maintained by their standalone repositories.
Python owns its migrated copies alongside its extension.

## Versioning

Python package and runtime versions follow the independent binding policy.
Core identity is recorded separately in the dependency lock and installed
metadata. See the standalone [versioning policy](https://github.com/OpenTLV/opentlv-python/blob/main/docs/versioning.md).

## Diagnostic vocabulary

Core owns `scripts/generate_diagnostic_vocabulary.py` because the canonical
enums and their names come from its public headers and C name tables. Its
default invocation and `--check` validate the Core vocabulary without writing
binding files.
To refresh or check the Python outputs against this Core checkout, explicitly
select the standalone checkout from the Core repository root:

```sh
python -B scripts/generate_diagnostic_vocabulary.py --python-output-root ../opentlv-python
python -B scripts/generate_diagnostic_vocabulary.py --check --python-output-root ../opentlv-python
```

The destination must already contain the standalone facade source and test
directories and must be outside the Core source tree. Generation does not
select a new Core dependency for Python. Review any vocabulary change together
with that repository's dependency update and run its installed-package tests.

## Continuous integration

The standalone [quality gates](https://github.com/OpenTLV/opentlv-python/blob/main/docs/quality-gates.md)
own Python PR/Main/Full validation. Core PR/Full workflows continue to own
native validation and shared fixtures; they do not require an installed Python
binding. Cross-language Query release evidence still requires Python results
from its owning repository.

## Migration and documentation cutover

The [frozen migration inventory](https://github.com/OpenTLV/opentlv-python/blob/main/docs/migration.md)
records Core commit `5e0aae9c77e9c22d07c7f03d6110243e9ecad85f`, including all
71 original Python subtree files and the migrated Query facade runner.
The workspace supplies `opentlv-python` as a sibling checkout. Canonical Python
guides now live there, and the portal consumes that standalone source. This
Core landing page records native integration and the retained migration context.
The transitional Core user guide remains available for existing source links;
use the standalone guide for maintained Python instructions.
