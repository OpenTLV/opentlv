"""Compare the installed libraries with the reviewed C function inventories."""

import os
import platform
import re
import subprocess
from pathlib import Path


BASELINE = Path(__file__).resolve().parent / "abi"


def tool_output(*command):
    try:
        return subprocess.check_output(command, text=True, errors="replace",
                                       env={**os.environ, "LC_ALL": "C", "VSLANG": "1033"})
    except FileNotFoundError:
        raise SystemExit(f"Missing symbol inspection tool: {command[0]}") from None


def nm_symbols(library, nm, shared, objdump):
    flags = ["-D"] if shared else []
    output = tool_output(nm, "-P", "-g", "--defined-only", *flags, library)
    names = set()
    runtime_addresses = {}
    if shared:
        # Some ELF CRTs export their loader hooks. Confirm each name and address
        # against DT_INIT/DT_FINI before separating it from the library API.
        dynamic = tool_output(objdump, "-p", library)
        for tag, address in re.findall(r"^[ \t]*(INIT|FINI)[ \t]+(0x[0-9a-fA-F]+)[ \t]*$",
                                       dynamic, re.MULTILINE):
            runtime_addresses["_" + tag.lower()] = int(address, 16)
    runtime = []
    for line in output.splitlines():
        if line.strip() and not line.endswith(":"):  # archive member headings
            name, kind, address, *_ = line.split()
            if kind == "T" and name in runtime_addresses and int(address, 16) == runtime_addresses[name]:
                runtime.append(name)
                continue
            names.add(name if kind == "T" else f"{name} ({kind})")
    if runtime:
        print(f"{library.name}: ELF runtime entries {', '.join(sorted(runtime))}")
    return names


def msvc_symbols(library, linker, shared):
    if shared:
        output = tool_output(linker, "/dump", "/nologo", "/exports", library)
        # Parse the DLL export directory, including entries exported only by ordinal.
        sections = re.split(r"\bordinal\s+hint\s+RVA\s+name\b", output, maxsplit=1)
        if len(sections) != 2:
            raise SystemExit(f"{library}: missing DLL export table")
        table = sections[1].strip().split("\n\n", 1)[0]
        names = set()
        for line in table.splitlines():
            fields = line.split()
            if len(fields) < 4:
                raise SystemExit(f"{library}: unexpected DLL export entry {line!r}")
            names.add(fields[3])
        return names

    output = tool_output(linker, "/dump", "/nologo", "/linkermember:1", "/headers", library)
    table = output.partition("public symbols")[2].strip().split("\n\n")[0]
    x86 = "14C machine (x86)" in output
    names = set()
    for line in table.splitlines():
        _, name = line.split(maxsplit=1)
        # MSVC places pooled string literals in the archive index as COMDATs.
        if not name.startswith("??_C@"):
            if x86 and name.startswith("_"):
                name = name[1:]
            names.add(name)
    return names


def mingw_exports(library, objdump):
    output = tool_output(objdump, "-p", library)
    addresses, _, names = output.partition("[Ordinal/Name Pointer] Table")
    # Binutils versions differ in the number of columns before the export name.
    rows = names.split("\n\n", 1)[0].splitlines()
    exported = {row.split()[-1] for row in rows if re.match(r"\s*\[\s*\d+\]", row)}
    address_count = len(re.findall(r"^\s*\[\s*\d+\]\s+\+base\[", addresses, re.MULTILINE))
    if len(exported) != address_count:
        raise SystemExit(f"{library}: expected a name for each of {address_count} exported ordinals")
    return exported


def check_symbols(prefix, shared, build):
    """Check Linux or Windows artifacts using the target's CMake tools."""
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    tools = dict(re.findall(r"^CMAKE_(NM|LINKER|OBJDUMP):FILEPATH=(.+)$", cache, re.MULTILINE))
    target = re.search(r"^CMAKE_SYSTEM_NAME:[^=]+=(.+)$", cache, re.MULTILINE)
    system = target[1] if target else platform.system()
    windows = system == "Windows"
    if system not in ("Windows", "Linux"):
        raise SystemExit("The symbol audit supports Linux ELF and Windows PE/COFF packages")
    msvc = windows and Path(tools.get("LINKER", "")).name.lower() == "link.exe"

    for module in ("core", "io"):
        inventory = (BASELINE / f"{module}.h").read_text(encoding="utf-8")
        expected = set(re.findall(r"^OPENTLV_ABI_FUNCTION\([^,]+,\s*(\w+),", inventory, re.MULTILINE))
        name = f"opentlv_{module}"
        if windows and shared:
            filenames = {name + ".dll", "lib" + name + ".dll"}
        elif msvc:
            filenames = {name + ".lib"}
        else:
            filenames = {"lib" + name + (".so" if shared else ".a")}
        matches = [p for p in prefix.rglob("*") if p.name in filenames and p.is_file()]
        if len(matches) != 1:
            raise SystemExit(f"{module}: expected one installed library matching {sorted(filenames)}, got {matches}")
        library = matches[0]
        if msvc:
            actual = msvc_symbols(library, tools["LINKER"], shared)
        elif windows and shared:
            actual = mingw_exports(library, tools["OBJDUMP"])
        else:
            actual = nm_symbols(library, tools["NM"], shared, tools.get("OBJDUMP", "objdump"))
            if windows and "file format pe-i386" in tool_output(tools["OBJDUMP"], "-f", library):
                actual = {s[1:] if s.startswith("_") else s for s in actual}
        if actual != expected:
            raise SystemExit(f"{library.name}: missing {sorted(expected - actual)}, "
                             f"unexpected {sorted(actual - expected)}")
        print(f"PASS {library.name}: {len(actual)} C symbols match the {module} baseline")
