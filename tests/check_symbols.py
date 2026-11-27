"""Compare the installed libraries with the reviewed C function inventories."""

import os
import platform
import re
import subprocess
from pathlib import Path

from release_baseline import BASELINE


def tool_output(*command):
    try:
        return subprocess.check_output(command, text=True, errors="replace",
                                       env={**os.environ, "LC_ALL": "C", "VSLANG": "1033"})
    except FileNotFoundError:
        raise SystemExit(f"Missing symbol inspection tool: {command[0]}") from None


def table_rows(library, output, heading, end):
    match = re.search(heading, output, re.MULTILINE)
    if not match:
        raise SystemExit(f"{library}: missing symbol table header matching {heading!r}")
    table = re.split(end, output[match.end():], maxsplit=1, flags=re.MULTILINE)[0]
    return [line for line in table.splitlines() if line.strip()]


def row_fields(library, line, pattern):
    match = re.fullmatch(pattern, line.strip())
    if not match:
        raise SystemExit(f"{library}: unexpected symbol table entry {line!r}")
    return match.groups()


def nm_symbols(library, nm, shared, objdump):
    flags = ["-D"] if shared else []
    output = tool_output(nm, "-P", "-g", "--defined-only", *flags, library)
    names = set()
    runtime_addresses = {}
    if shared:
        # Some ELF CRTs export their loader hooks. Confirm each name and address
        # against DT_INIT/DT_FINI before separating it from the library API.
        dynamic = tool_output(objdump, "-p", library)
        if "Dynamic Section:" not in dynamic:
            raise SystemExit(f"{library}: missing ELF dynamic section")
        for line in dynamic.splitlines():
            if re.match(r"\s*(?:INIT|FINI)\b", line):
                tag, address = row_fields(library, line, r"(INIT|FINI)\s+(0x[0-9a-fA-F]+)")
                runtime_addresses["_" + tag.lower()] = int(address, 16)
    runtime = []
    member = r".+(?:\[[^]]+\]|\([^)]+\)|\.o(?:bj)?):"
    symbol = r"(\S+)\s+([A-Za-z?])\s+([0-9a-fA-F]+)(?:\s+[0-9a-fA-F]+)?"
    for line in output.splitlines():
        if not line.strip() or (not shared and re.fullmatch(member, line.strip())):
            continue  # blank lines and archive member headings
        name, kind, address = row_fields(library, line, symbol)
        if kind == "T" and name in runtime_addresses and int(address, 16) == runtime_addresses[name]:
            runtime.append(name)
            continue
        names.add(name if kind == "T" else f"{name} ({kind})")
    if not names:
        raise SystemExit(f"{library}: no external symbols found")
    if runtime:
        print(f"{library.name}: ELF runtime entries {', '.join(sorted(runtime))}")
    return names


def msvc_symbols(library, linker, shared):
    if shared:
        output = tool_output(linker, "/dump", "/nologo", "/exports", library)
        heading = r"^[ \t]*ordinal\s+hint\s+RVA\s+name[ \t]*$"
        rows = table_rows(library, output, heading, r"^[ \t]*Summary[ \t]*$")
        # The function count includes gaps in the export ordinal range.
        count = re.search(r"^[ \t]*(\d+)[ \t]+number of names[ \t]*$", output, re.MULTILINE)
        pattern = r"\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)(?:\s+=\s+\S.*)?"
    else:
        output = tool_output(linker, "/dump", "/nologo", "/linkermember:1", "/headers", library)
        heading = r"^[ \t]*(\d+) public symbols?[ \t]*$"
        rows = table_rows(library, output, heading,
                          r"^FILE HEADER VALUES|^Archive member name at |^[ \t]*Summary[ \t]*$")
        count = re.search(heading, output, re.MULTILINE)
        pattern = r"[0-9A-Fa-f]+\s+(\S+)"
    names = [row_fields(library, line, pattern)[0] for line in rows]
    if not names or not count or len(names) != int(count[1]):
        raise SystemExit(f"{library}: symbol table disagrees with MSVC entry count")
    if not shared:
        # MSVC places pooled string literals in the archive index as COMDATs.
        names = [name for name in names if not name.startswith("??_C@")]
        if "14C machine (x86)" in output:
            names = [name[1:] if name.startswith("_") else name for name in names]
    return set(names)


def mingw_exports(library, objdump):
    output = tool_output(objdump, "-p", library)
    name_heading = r"^\[Ordinal/Name Pointer\] Table(?: -- Ordinal Base \d+)?[ \t]*$"
    address_heading = r"^Export Address Table -- Ordinal Base \d+[ \t]*$"
    addresses = table_rows(library, output, address_heading, name_heading)
    names = table_rows(library, output, name_heading, r"^(?:The |PE File |There is a |Dump of )")
    # Newer Binutils print column titles and add ordinal/hint columns to names.
    index = r"\[\s*(\d+)\]"
    base = r"\+base\[\s*\d+\]\s+[0-9A-Fa-f]+"
    address = index + r"\s+" + base + r"\s+(?:Export RVA|Forwarder RVA -- \S+)"
    ordinals = {int(row_fields(library, line, address)[0])
                for line in addresses if line.split() != ["Ordinal", "Address", "Type"]}
    entries = [row_fields(library, line, index + r"\s+(?:" + base + r"\s+)?(\S+)")
               for line in names if line.split() != ["Ordinal", "Hint", "Name"]]
    counts = re.search(r"Export Address Table[ \t]+([0-9a-fA-F]+)\n\s*"
                       r"\[Name Pointer/Ordinal\] Table[ \t]+([0-9a-fA-F]+)\b", output)
    if (not counts or not ordinals or max(ordinals) >= int(counts[1], 16)
            or len(entries) != int(counts[2], 16) or {int(ordinal) for ordinal, _ in entries} != ordinals):
        raise SystemExit(f"{library}: inconsistent MinGW export address and name tables")
    return {name for _, name in entries}


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
