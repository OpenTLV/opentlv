"""Check the public OpenTLV headers with C99 and C++11 compilers."""

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

UMBRELLA_PROBE = """\
#include <opentlv/tlv.h>
#if TLV_C99 != 199901L || TLV_C_VERSION != TLV_C99
#error "Expected the OpenTLV C99 baseline"
#endif
TLV_NODISCARD int check_result(void);
"""


def check(build):
    include = build / "include"
    shutil.copytree(ROOT / "include" / "opentlv", include / "opentlv")
    headers = sorted(path.relative_to(include).as_posix() for path in include.rglob("*.h"))
    # The declaration also makes a macros-only header a nonempty C99 translation unit.
    probes = [
        f"#include <{header}>\n" * 2 + "int header_check(void);\n"
        for header in headers
    ]
    probes.append(UMBRELLA_PROBE)
    compilers = [
        (os.environ.get("CC", "cc"), "c99", "check.c"),
        (os.environ.get("CXX", "c++"), "c++11", "check.cpp"),
    ]
    for compiler, standard, filename in compilers:
        command = [
            compiler, "-std=" + standard, "-pedantic-errors", "-Wall", "-Wextra",
            "-Werror", "-I", str(include), "-c", filename, "-o", "check.o",
        ]
        for source in probes:
            (build / filename).write_text(source, encoding="utf-8")
            subprocess.run(command, cwd=build, check=True)
        print(f"{standard}: {len(headers)} public headers and the umbrella API passed")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        check(Path(directory))
