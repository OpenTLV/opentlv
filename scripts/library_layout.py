# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Read historical release layouts without restoring legacy installed headers."""

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def public_roots(source):
    source = Path(source)
    if (source / "include/opentlv").is_dir():
        return [source / "include"]
    return [source / "tlv/include", source / "tlv++/include"]


def historical_includes(source, text):
    """Adapt a current probe to an immutable pre-migration source checkout."""
    if (Path(source) / "include/opentlv").is_dir():
        return text
    mapping = json.loads((ROOT / "docs/development/modules/header-paths.json").read_text())
    reverse = {new: old for old, new in mapping.items()}
    return re.sub(r'(#[ \t]*include[ \t]*[<"])([^>"\n]+)([>"])',
                  lambda match: match[1] + reverse.get(match[2], match[2]) + match[3], text)


def historical_header(source, header):
    if not header.startswith("include/opentlv/") or (Path(source) / "include/opentlv").is_dir():
        return header
    name = historical_includes(source, f'#include "{header.removeprefix("include/")}"').split('"')[1]
    return ("tlv++/include/" if name.endswith(".hpp") else "tlv/include/") + name
