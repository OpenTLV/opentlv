#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Check the functional module contracts of #644 against the source tree.

docs/development/modules/ownership.json assigns every C/C++ source, header and
resource template to one target and declares the acyclic target graph. An
include is permitted within its target or along a declared dependency; a
private header under tlv/src is permitted only within the module that owns it,
including that module's integration targets. Each existing exception names the
approved resolution that removes it, and both new and stale exceptions fail.

docs/development/modules/diagnostic-mapping.json must cover every enumerator
of the public diagnostic vocabularies and every Query limit name, without
merging distinct codes. No native build is required.
"""

import argparse
import json
import re
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = "docs/development/modules/ownership.json"
MAPPING = "docs/development/modules/diagnostic-mapping.json"
SOURCE_ROOTS = ("tlv", "tlv++")
SUFFIXES = frozenset((".c", ".h", ".hpp", ".in"))
INCLUDE_ROOTS = ("tlv/include", "tlv++/include")
GENERIC = ("core", "io", "document", "query", "schema", "codec", "generator")
BUILTIN = ("asn1", "emv", "bluetooth", "nfc", "dhcp", "lldp")
ROLES = frozenset(("module", "integration", "aggregate"))
DEPENDENCY_LISTS = ("public", "private", "facade")
KINDS = frozenset(("dependency", "private-header"))
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"\n]+)"', re.M)
ENUM = r"typedef\s+enum\s+%s\s*\{(.*?)\}\s*%s_t\s*;"
LIMIT = re.compile(r'\b(?:query_limit|edit_limit)\s*\(\s*[^,]+,\s*"([a-z0-9-]+)"')
MAPPING_FIELDS = frozenset(("previous", "value", "new", "module", "meaning", "change"))


def read(root, relative):
    return (root / relative).read_text(encoding="utf-8")


def inventory(root):
    return {path.relative_to(root).as_posix()
            for base in SOURCE_ROOTS for path in (root / base).rglob("*")
            if path.is_file() and path.suffix in SUFFIXES}


def direct(entry, facade=False):
    """Direct dependencies of C sources, or of C++ facade headers when facade is set."""
    lists = DEPENDENCY_LISTS if facade else DEPENDENCY_LISTS[:2]
    return [dependency for key in lists for dependency in entry.get(key, ())]


def closure(targets, name, facade=False):
    seen, stack = set(), direct(targets[name], facade)
    while stack:
        current = stack.pop()
        if current not in seen:
            seen.add(current)
            stack.extend(direct(targets[current], facade))
    return seen


def cycles(graph):
    """Return the strongly connected components that contain a cycle (Tarjan)."""
    index, low, stack, on_stack, found = {}, {}, [], set(), []

    def visit(node):
        index[node] = low[node] = len(index)
        stack.append(node)
        on_stack.add(node)
        for target in sorted(graph.get(node, ())):
            if target not in index:
                visit(target)
                low[node] = min(low[node], low[target])
            elif target in on_stack:
                low[node] = min(low[node], index[target])
        if low[node] == index[node]:
            component = []
            while True:
                member = stack.pop()
                on_stack.discard(member)
                component.append(member)
                if member == node:
                    break
            if len(component) > 1 or node in graph.get(node, ()):
                found.append(sorted(component))

    for node in sorted(graph):
        if node not in index:
            visit(node)
    return sorted(found)


def check_contract(contract, errors):
    modules, targets = contract.get("modules", {}), contract.get("targets", {})
    kinds = {kind: sorted(name for name, entry in modules.items() if entry.get("kind") == kind)
             for kind in ("generic", "builtin")}
    if kinds["generic"] != sorted(GENERIC) or kinds["builtin"] != sorted(BUILTIN):
        errors.append("modules: expected the seven generic and six builtin public modules")
    for name, entry in targets.items():
        role, module = entry.get("role"), entry.get("module")
        if role not in ROLES:
            errors.append(f"target {name}: unknown role {role!r}")
        elif role == "aggregate" and module is not None:
            errors.append(f"target {name}: the aggregate belongs to no module")
        elif role != "aggregate" and module not in modules:
            errors.append(f"target {name}: unknown owning module {module!r}")
        elif role == "module" and module != name:
            errors.append(f"target {name}: a module target is named after its module")
        listed = direct(entry, facade=True)
        if len(set(listed)) != len(listed) or name in listed:
            errors.append(f"target {name}: list each direct dependency once and never itself")
        for dependency in listed:
            if dependency not in targets:
                errors.append(f"target {name}: unknown dependency {dependency}")
            elif targets[dependency]["role"] == "aggregate":
                errors.append(f"target {name}: no target depends on the aggregate")
            elif module in GENERIC and targets[dependency]["module"] in BUILTIN:
                errors.append(f"target {name}: a generic target depends on builtin {dependency}")
    for module in modules:
        if targets.get(module, {}).get("role") != "module":
            errors.append(f"module {module}: missing its module target")
    if errors:
        return
    # Facade dependencies extend the C graph, so an acyclic facade graph covers both.
    graph = {name: set(direct(entry, facade=True)) for name, entry in targets.items()}
    for component in cycles(graph):
        errors.append(f"targets: dependency cycle {' -> '.join(component)}")
    for name, entry in targets.items():
        if entry["role"] == "integration" and entry["module"] not in closure(targets, name):
            errors.append(f"target {name}: an integration depends on its owning module")


def resolve(root, including, name):
    candidates = [PurePosixPath(including).parent / name]
    candidates += [PurePosixPath(base) / name for base in INCLUDE_ROOTS]
    for candidate in candidates:
        parts = []
        for part in candidate.parts:
            if part == "..":
                if parts:
                    parts.pop()
            elif part != ".":
                parts.append(part)
        relative = "/".join(parts)
        if (root / relative).is_file():
            return relative
    return None


def edges(root, contract, files, errors):
    """Yield (file, include, resolved owner target) for every quoted include."""
    generated = contract.get("generated", {})
    for path in sorted(files):
        if path.endswith(".in"):
            continue
        for name in INCLUDE.findall(COMMENT.sub("", read(root, path))):
            if name in generated:
                yield path, name, generated[name], name
                continue
            resolved = resolve(root, path, name)
            if resolved is None:
                errors.append(f"{path}: cannot resolve include \"{name}\"")
            elif resolved in files:
                yield path, name, files[resolved], resolved


def violations(root, contract, errors):
    files, targets = contract["files"], contract["targets"]
    reachable = {(name, facade): closure(targets, name, facade) | {name}
                 for name in targets for facade in (False, True)}
    found, graph = set(), {}
    for path, name, owner, resolved in edges(root, contract, files, errors):
        source = files[path]
        if owner != source:
            graph.setdefault(source, set()).add(owner)
        allowed = reachable[source, path.endswith(".hpp")]
        if targets[source]["role"] != "aggregate" and owner not in allowed:
            found.add((path, name, "dependency"))
        if (resolved.startswith("tlv/src/") and source != owner
                and targets[source]["module"] != targets[owner]["module"]):
            found.add((path, name, "private-header"))
    return found, graph


def check_files(root, contract, errors):
    files, targets = contract.get("files", {}), contract.get("targets", {})
    actual = inventory(root)
    missing, stale = sorted(actual - set(files)), sorted(set(files) - actual)
    if missing:
        errors.append(f"files: assign an owner to {', '.join(missing)}")
    if stale:
        errors.append(f"files: remove stale {', '.join(stale)}")
    for path, owner in sorted(files.items()):
        if owner not in targets:
            errors.append(f"files: {path} has unknown owner {owner}")
        elif targets[owner].get("facade_only") and not path.endswith(".hpp"):
            errors.append(f"files: facade-only target {owner} cannot own {path}")
    for name, owner in sorted(contract.get("generated", {}).items()):
        if owner not in targets:
            errors.append(f"generated: {name} has unknown owner {owner}")
    # A moved declaration or definition keeps its current file until its resolution lands.
    text = "\n".join(read(root, path) for path in sorted(actual) if not path.endswith(".hpp"))
    for symbol, owner in sorted(contract.get("api", {}).items()):
        if owner.get("target") is not None and owner.get("target") not in targets:
            errors.append(f"api: {symbol} has unknown target {owner.get('target')}")
        if owner.get("resolution") not in contract.get("resolutions", {}):
            errors.append(f"api: {symbol} names unknown resolution {owner.get('resolution')}")
        if not re.search(r"\b%s\b" % re.escape(symbol), text):
            errors.append(f"api: remove stale {symbol}")


def check_exceptions(root, contract, errors):
    resolutions = contract.get("resolutions", {})
    used = {entry.get("resolution") for entry in contract.get("exceptions", ())}
    used |= {entry.get("resolution") for entry in contract.get("api", {}).values()}
    for key, entry in resolutions.items():
        for required in entry.get("requires", ()):
            if required not in resolutions:
                errors.append(f"resolutions: {key} requires unknown {required}")
            used.add(required)
    for key, entry in sorted(resolutions.items()):
        if not isinstance(entry.get("issue"), int) or not entry.get("summary", "").strip():
            errors.append(f"resolutions: {key} needs an implementing issue and a summary")
        if key not in used:
            errors.append(f"resolutions: remove {key}, which no exception or API move uses")
    documented = set()
    for entry in contract.get("exceptions", ()):
        kind, resolution = entry.get("kind"), entry.get("resolution")
        if kind not in KINDS or resolution not in resolutions:
            errors.append(f"exceptions: {entry.get('include')} needs a kind and known resolution")
        for path in entry.get("files", ()):
            item = (path, entry.get("include"), kind)
            if item in documented:
                errors.append(f"exceptions: duplicate {path}: {item[1]} ({kind})")
            documented.add(item)
    actual, graph = violations(root, contract, errors)
    for path, name, kind in sorted(actual - documented):
        errors.append(f"{path}: {kind} violation including \"{name}\"; "
                      "follow the module contract or record an approved resolution")
    for path, name, kind in sorted(documented - actual):
        errors.append(f"exceptions: remove resolved {kind} exception {path}: \"{name}\"")
    return graph


def parse_enum(root, header, typedef):
    match = re.search(ENUM % (typedef, typedef), COMMENT.sub("", read(root, header)), re.S)
    if not match:
        raise ValueError(f"{header}: enum {typedef} not found")
    members, value = [], -1
    for entry in filter(None, (part.strip() for part in match[1].split(","))):
        name, _, explicit = (text.strip() for text in entry.partition("="))
        value = int(explicit, 0) if explicit else value + 1
        members.append((name, value))
    return members


def limit_names(root):
    names = set()
    for path in sorted((root / "tlv/src").rglob("*.c")):
        names.update(LIMIT.findall(path.read_text(encoding="utf-8")))
    return names


def check_mapping(root, contract, mapping, errors):
    domains, modules = mapping.get("domains", {}), contract.get("modules", {})
    identifiers = {}
    for name, entry in sorted(domains.items()):
        if entry.get("module") not in modules:
            errors.append(f"domains: {name} has unknown module {entry.get('module')}")
        identifier = entry.get("id")
        if not isinstance(identifier, int) or identifier <= 0:
            errors.append(f"domains: {name} needs a positive id")
        elif identifier in identifiers:
            errors.append(f"domains: {name} reuses id {identifier:#06x} of {identifiers[identifier]}")
        else:
            identifiers[identifier] = name
        low, high = mapping.get("ranges", {}).get(modules.get(entry.get("module"), {}).get("kind"),
                                                  (0, -1))
        if isinstance(identifier, int) and not low <= identifier <= high:
            errors.append(f"domains: {name} id {identifier:#06x} is outside its range")
    new_codes, family_ids = {}, {}
    for family in mapping.get("families", ()):
        label = family.get("family", "?")
        if family.get("domain") not in domains:
            errors.append(f"{label}: unknown domain {family.get('domain')}")
            continue
        number = family.get("id")
        if number is not None:
            if not isinstance(number, int) or number <= 0:
                errors.append(f"{label}: a family id is a positive integer or null")
            elif (family["domain"], number) in family_ids:
                errors.append(f"{label}: reuses family id {number} of "
                              f"{family_ids[family['domain'], number]}")
            family_ids[family["domain"], number] = label
        owner = domains[family["domain"]]["module"]
        codes = family.get("codes", [])
        if "typedef" in family:
            target = contract["files"].get(family.get("header"))
            if target is None or contract["targets"][target]["module"] != owner:
                errors.append(f"{label}: {family.get('header')} is not a header of {owner}")
            try:
                expected = parse_enum(root, family["header"], family["typedef"])
            except (OSError, ValueError) as error:
                errors.append(f"{label}: {error}")
                continue
            listed = [(code.get("previous"), code.get("value")) for code in codes]
            for name, value in sorted(set(expected) - set(listed)):
                errors.append(f"{label}: map {name} = {value}")
            for name, value in sorted(set(listed) - set(expected)):
                errors.append(f"{label}: remove or correct stale {name} = {value}")
        else:
            expected = limit_names(root)
            listed = {code.get("previous") for code in codes}
            for name in sorted(expected - listed):
                errors.append(f"{label}: map limit name \"{name}\"")
            for name in sorted(listed - expected):
                errors.append(f"{label}: remove stale limit name \"{name}\"")
        for code in codes:
            name = code.get("previous")
            if set(code) != MAPPING_FIELDS:
                errors.append(f"{label}: {name} needs exactly {', '.join(sorted(MAPPING_FIELDS))}")
                continue
            # A limit name may be produced by another module's integration into a Query diagnostic.
            if code["module"] != owner and ("typedef" in family or code["module"] not in modules):
                errors.append(f"{label}: {name} cannot belong to {code['module']}")
            if not all(isinstance(code[key], str) and code[key].strip()
                       for key in ("meaning", "change", "new")):
                errors.append(f"{label}: {name} needs a new code, meaning and change")
                continue
            key = (family["domain"], family["family"], code["new"])
            if key in new_codes:
                errors.append(f"{label}: {name} and {new_codes[key]} merge into {code['new']}")
            new_codes[key] = name


def check(root):
    errors = []
    try:
        contract = json.loads(read(root, CONTRACT))
        mapping = json.loads(read(root, MAPPING))
    except (OSError, ValueError) as error:
        return [f"cannot load the module contracts: {error}"], {}
    check_contract(contract, errors)
    if errors:
        return errors, {}
    check_files(root, contract, errors)
    graph = check_exceptions(root, contract, errors) if not errors else {}
    check_mapping(root, contract, mapping, errors)
    return errors, graph


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", action="store_true",
                        help="print current target-level include cycles")
    args = parser.parse_args()
    errors, graph = check(args.root.resolve())
    if args.report:
        for component in cycles(graph):
            print("current cycle: " + " <-> ".join(component))
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if errors:
        return 1
    print("Module contracts match the source tree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
