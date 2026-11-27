"""Compare the public header inventory with the v0.1.0 release baseline."""

from pathlib import Path


BASELINE = Path(__file__).resolve().parent / "baselines" / "v0.1.0"


def public_headers(include):
    headers = {path.relative_to(include).as_posix() for path in include.rglob("*")
               if path.is_file() and path.suffix in (".h", ".hpp")}
    expected = set((BASELINE / "headers.txt").read_text(encoding="utf-8").splitlines())
    if headers != expected:
        raise SystemExit(f"{include}: missing public headers {sorted(expected - headers)}, "
                         f"unexpected {sorted(headers - expected)}")
    return sorted(headers)
