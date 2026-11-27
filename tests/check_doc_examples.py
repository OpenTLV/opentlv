"""Extract the documented consumer examples for release package checks."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
# Read each complete fence as one token, including any heading-like lines inside.
MARKDOWN_BLOCKS = re.compile(r"^```([^`\n]*)\n(.*?)^```[ \t]*$|^#{1,6} ([^\n]+)$",
                             re.MULTILINE | re.DOTALL)


def code_block(document, heading, language):
    """Return the single fenced block of this language directly under the heading."""
    selected = False
    examples = []
    for token in MARKDOWN_BLOCKS.finditer(document.read_text(encoding="utf-8")):
        fence_language, source, title = token.groups()
        if title is not None:
            selected = title == heading
        elif selected and fence_language == language:
            examples.append(source)
    if len(examples) != 1:
        raise SystemExit(f"{document}: expected one {language} example under {heading!r}, "
                         f"found {len(examples)}")
    return examples[0]


def write_cpp_examples(directory):
    document = ROOT / "docs" / "cpp-api.md"
    directory.mkdir(parents=True, exist_ok=True)
    for module, heading in (("core", "Using Fixed TLV from C++17"), ("io", "Reading elements")):
        source = code_block(document, heading, "cpp")
        (directory / f"{module}.cpp").write_text(source, encoding="utf-8")


def readme_outputs(readme, sources):
    """Read the example source paths and literal stdout in the README table."""
    row = (r"^\|[ \t]*\[[^\]\n]+\]\((examples/[^)\s]+)\)[ \t]*"
           r"\|[ \t]*`([^`\n]+)`[ \t]*\|[ \t]*$")
    rows = re.findall(row, readme.read_text(encoding="utf-8"), re.MULTILINE)
    outputs = dict(rows)
    if len(rows) != len(sources) or outputs.keys() != sources:
        raise SystemExit(f"{readme}: expected one output row for each of {sorted(sources)}")
    return outputs
