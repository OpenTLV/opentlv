"""Check release documentation links with the OpenTLV organization checker."""

import argparse
from pathlib import Path
import re
import sys
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]


def check(root, governance, checker):
    documents = sorted([*root.glob("*.md"), *(root / "docs").rglob("*.md"),
                        *(root / "tests").rglob("*.md")])
    errors = []
    checked = 0
    for source in documents:
        text = checker.without_code_spans(checker.without_fences(source.read_text(encoding="utf-8")))
        # The shared extractor expects the label of an inline link on one line.
        text = re.sub(r"\[[^\[\]]*\](?=\()", lambda match: match[0].replace("\n", " "), text)
        for target in checker.markdown_links(text):
            url = urlsplit(target)
            if not url.scheme and not url.netloc:
                destination_root = root
            elif (url.scheme in ("http", "https") and url.netloc.lower() == "github.com"
                  and re.match(checker.REPOSITORY_FILES, url.path)):
                destination_root = governance
            else:
                continue
            checked += 1
            for error in checker.check_link_targets(destination_root, source, [target]):
                errors.append(f"{source.relative_to(root).as_posix()}: {error}")
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"Checked {checked} local and organization link targets in {len(documents)} Markdown files.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("governance", type=Path, help="path to the OpenTLV/.github v0.1.0 checkout")
    governance = parser.parse_args().governance.resolve()
    scripts = governance / "scripts"
    if not (scripts / "check_community.py").is_file():
        parser.error(f"missing organization checker in {scripts}")
    sys.path.insert(0, str(scripts))
    import check_community

    check(ROOT, governance, check_community)
