#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Check the functional module contracts of #644 against the source tree.

docs/development/modules/ownership.json assigns every C/C++ source, header and
resource template to one target and declares the acyclic target graph. An
include is permitted within its target or along a declared dependency; a
private header under src is permitted only within the module that owns it,
including that module's integration targets. Each existing exception names the
approved resolution that removes it, and both new and stale exceptions fail.

docs/development/modules/diagnostic-mapping.json must cover every enumerator
of the public diagnostic vocabularies and every Query limit name, without
merging distinct codes. No native build is required. The optional example audit
uses a registry emitted by CMake configuration to check public includes against
direct public target links, including composite headers.
"""

import argparse
import json
import re
import sys
from pathlib import Path, PurePosixPath

from check_cxx_boundary import header_source

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = "docs/development/modules/ownership.json"
MAPPING = "docs/development/modules/diagnostic-mapping.json"
SOURCE_ROOTS = ("include", "src")
SUFFIXES = frozenset((".c", ".h", ".hpp", ".in"))
INCLUDE_ROOTS = ("include", ".")
GENERIC = ("core", "io", "document", "query", "schema", "codec", "generator")
BUILTIN = ("asn1", "emv", "bluetooth", "nfc", "dhcp", "lldp")
ROLES = frozenset(("module", "integration", "aggregate"))
DEPENDENCY_LISTS = ("public", "private", "facade")
KINDS = frozenset(("dependency", "private-header"))
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"\n]+)[>"]', re.M)
INCLUDE_DIRECTIVE = re.compile(r'^[ \t]*#[ \t]*include\b([^\n]*)', re.M)
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
    public_targets(contract, errors)
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
            elif (module in BUILTIN and targets[dependency]["module"] in BUILTIN
                  and targets[dependency]["module"] != module):
                errors.append(f"target {name}: a builtin depends on another builtin {dependency}")
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
        resolved = (root / candidate).resolve()
        if resolved.is_file() and resolved.is_relative_to(root.resolve()):
            return resolved.relative_to(root.resolve()).as_posix()
    return None


def edges(root, contract, files, errors):
    """Resolve literal includes before checking ownership, including ../ bypasses."""
    generated = contract.get("generated", {})
    for path in sorted(files):
        source = COMMENT.sub("", read(root, path).replace("\\\n", ""))
        for directive in INCLUDE_DIRECTIVE.finditer(source):
            if not INCLUDE.match(directive[0]):
                errors.append(f"{path}: use a literal header name so include ownership can be checked")
        for delimiter, name in INCLUDE.findall(source):
            if name in generated:
                yield path, name, generated[name], name
                continue
            resolved = resolve(root, path, name)
            if resolved is None and (delimiter == '"' or name.startswith(("opentlv/", "src/", "."))):
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
        if (resolved.startswith("src/") and
                (path.startswith("include/") or
                 targets[source]["module"] != targets[owner]["module"])):
            found.add((path, name, "private-header"))
    return found, graph


def check_files(root, contract, errors):
    files, targets = contract.get("files", {}), contract.get("targets", {})
    actual = inventory(root)
    for legacy in ("tlv", "tlv++"):
        if (root / legacy).exists():
            errors.append(f"layout: historical source root {legacy} must not exist")
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
        else:
            module = targets[owner]["module"]
            base = f"builtin/{module}" if module in BUILTIN else module
            if path.startswith("include/"):
                prefix = f"include/opentlv/{base}/" if base else "include/opentlv/"
            else:
                prefix = f"src/{base}/"
                if path.endswith((".h", ".h.in")):
                    prefix += "internal/"
            if not path.startswith(prefix):
                errors.append(f"layout: {path} must belong under {prefix}")
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
    # Baseline capture also reads immutable pre-migration release sources.
    source = root / "src" if (root / "src").is_dir() else root / "tlv/src"
    for path in sorted(source.rglob("*.c")):
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


def check_baseline_mapping(baseline, mapping, errors):
    """Require the immutable old vocabulary even if a current enum disappears."""
    families = {}
    for family in mapping.get("families", []):
        name = family["family"]
        if name in families:
            errors.append(f"baseline: duplicate family {name}")
        families[name] = family
    coverage = []
    for previous in baseline:
        name = previous["family"]
        family = families.get(name, {})
        codes = family.get("codes", [])
        for old, value in previous["codes"].items():
            matches = [code for code in codes if code.get("previous") == old]
            if len(matches) != 1 or matches[0].get("value") != value:
                errors.append(f"baseline: missing or changed mapping {name}/{old} = {value}")
                continue
            code = matches[0]
            if family.get("baseline") is False:
                errors.append(f"baseline: {name} cannot opt out of reference coverage")
            coverage.append({"family": name, **code})
    return coverage


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
    baseline = root / "tests/baseline/v0.8.0/diagnostic-codes.json"
    if baseline.is_file():
        check_baseline_mapping(json.loads(baseline.read_text(encoding="utf-8")), mapping, errors)
    return errors, graph


def public_targets(contract, errors):
    """Public target identities live alongside the existing ownership contract."""
    names = {}
    for key, entry in contract.get("targets", {}).items():
        name = entry.get("public_target", contract.get("modules", {}).get(key, {}).get("public_target"))
        if not isinstance(name, str) or not re.fullmatch(r"OpenTLV::[A-Za-z][A-Za-z0-9_+-]*", name):
            errors.append(f"target {key}: declare its public_target in ownership.json")
        elif name in names.values():
            errors.append(f"target {key}: duplicate public target {name}")
        else:
            names[key] = name
    return names


def example_includes(root, path, contract, errors):
    """Classify literal public and example-local includes without following library internals."""
    source = header_source(read(root, path).replace("\\\n", ""))
    for directive in INCLUDE_DIRECTIVE.finditer(source):
        if not INCLUDE.match(directive[0]):
            errors.append(f"{path}: use a literal header name so example dependencies can be checked")
    for delimiter, name in INCLUDE.findall(source):
        resolved = resolve(root, path, name)
        owner = contract.get("generated", {}).get(name)
        if owner is not None and name.startswith("opentlv/"):
            yield owner, name
        elif resolved and resolved.startswith("examples/"):
            yield None, resolved
        elif (resolved and resolved.startswith("include/opentlv/")
              and not any(part in PurePosixPath(resolved).parts for part in ("detail", "internal"))):
            if name != resolved.removeprefix("include/"):
                errors.append(f"{path}: use the public include spelling {resolved.removeprefix('include/')}")
            owner = contract["files"].get(resolved)
            if owner is None:
                errors.append(f"{path}: public header {name} has no owner")
            else:
                yield owner, resolved
        elif resolved:
            errors.append(f"{path}: private or non-public example include {name}")
        elif delimiter == '"' or name.startswith(("opentlv/", "src/", ".", "/")):
            errors.append(f"{path}: cannot resolve example include {name}")


def check_examples(root, registry, contract):
    """Require exact direct owners, including composite headers, from CMake declarations."""
    errors = []
    public = public_targets(contract, errors)
    if errors:
        return errors
    try:
        entries = [json.loads(line) for line in registry.read_text(encoding="utf-8").splitlines() if line.strip()]
    except (OSError, ValueError) as error:
        return [f"cannot load example registry {registry}: {error}"]
    registered, targets = set(), set()
    fields = {"target", "language", "sources", "link", "umbrella", "native_interop"}
    for entry in entries:
        if (not isinstance(entry, dict) or set(entry) != fields
                or not isinstance(entry["target"], str) or not entry["target"].startswith("example-")
                or entry["language"] not in ("c", "cpp")
                or any(type(entry[key]) is not bool for key in ("umbrella", "native_interop"))
                or any(not isinstance(entry[key], list) or not entry[key]
                       or any(not isinstance(item, str) or not item for item in entry[key])
                       for key in ("sources", "link"))):
            errors.append("examples: malformed CMake registry entry")
            continue
        target = entry["target"]
        if target in targets:
            errors.append(f"examples: duplicate target {target}")
        targets.add(target)
        required, pending, visited, composites = set(), [], set(), set()
        for source in entry["sources"]:
            path = PurePosixPath(source)
            if (path.is_absolute() or ".." in path.parts or "\\" in source or ":" in source
                    or path.as_posix() != source):
                errors.append(f"{target}: source must stay inside examples/: {source}")
                continue
            relative = "examples/" + source
            resolved = (root / relative).resolve()
            if not resolved.is_relative_to((root / "examples").resolve()) or not resolved.is_file():
                errors.append(f"{target}: missing example source {source}")
                continue
            if relative in registered:
                errors.append(f"{target}: duplicate example source {source}")
            registered.add(relative)
            if path.suffix in (".c", ".cpp"):
                expected = ".c" if entry["language"] == "c" else ".cpp"
                if path.suffix != expected:
                    errors.append(f"{target}: {source} does not match language {entry['language']}")
            pending.append(relative)
        while pending:
            path = pending.pop()
            if path in visited:
                continue
            visited.add(path)
            for owner, included in example_includes(root, path, contract, errors):
                if owner is None:
                    pending.append(included)
                elif contract["targets"][owner]["role"] != "aggregate":
                    required.add(owner)
                elif included == "include/opentlv/tlv.hpp" and entry["umbrella"]:
                    required.add(owner)
                    composites.add("umbrella")
                elif included == "include/opentlv/native.hpp" and entry["native_interop"]:
                    # native.hpp has no module-specific replacement. Derive its
                    # composite owners from its public includes, never a second map.
                    pending.append(included)
                    composites.add("native_interop")
                else:
                    errors.append(f"{target}: use module-specific headers instead of {included}")
        for flag in ("umbrella", "native_interop"):
            if entry[flag] and flag not in composites:
                errors.append(f"{target}: stale {flag} declaration")
        declared = set(entry["link"])
        if len(declared) != len(entry["link"]):
            errors.append(f"{target}: duplicate LINK target")
        for name in sorted(declared - set(public.values())):
            errors.append(f"{target}: unknown public LINK target {name}")
        expected = {public[owner] for owner in required}
        for name in sorted(expected - declared):
            errors.append(f"{target}: direct public includes require LINK {name}")
        for name in sorted(declared - expected):
            errors.append(f"{target}: unused LINK {name}; rely on exported transitive dependencies")
    sources = {path.relative_to(root).as_posix() for path in (root / "examples").rglob("*")
               if path.is_file() and path.suffix in (".c", ".cpp")}
    for source in sorted(sources - registered):
        errors.append(f"examples: source is absent from the CMake registry: {source}")
    if not entries:
        errors.append("examples: empty CMake registry")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", action="store_true",
                        help="print current target-level include cycles")
    parser.add_argument("--examples", type=Path,
                        help="audit direct example includes/LINK targets using helper-emitted examples.jsonl")
    args = parser.parse_args()
    errors, graph = check(args.root.resolve())
    if args.examples and not errors:
        contract = json.loads(read(args.root, CONTRACT))
        errors.extend(check_examples(args.root.resolve(), args.examples, contract))
    if args.report:
        for component in cycles(graph):
            print("current cycle: " + " <-> ".join(component))
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    if errors:
        return 1
    print("Module contracts match the source tree.")
    if args.examples:
        print("Example public includes match their direct CMake LINK targets.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
