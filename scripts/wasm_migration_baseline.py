# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel
"""Verify the immutable WASM extraction inventory; optionally reproduce its build."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RECORDS = ROOT / "docs/development/wasm-migration"
BASELINE = "a9a6f9470c2b285744738b40196bef9d0341fead"
OUTPUTS = ("opentlv-core.js", "opentlv-core.wasm", "opentlv.mjs", "query.mjs")
INPUTS = ("bindings/wasm", "bindings/common", "tlv", "cmake", "CMakeLists.txt",
          "LICENSE", ".github/workflows/wasm.yml", ".github/workflows/docs-notify.yml",
          "tests/query", "docs/development/webassembly.md", "docs/concepts/bindings.md")
FLAGS = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DOPENTLV_BUILD_WASM=ON",
         "-DOPENTLV_BUILD_CXX=OFF", "-DOPENTLV_BUILD_TESTS=OFF",
         "-DOPENTLV_BUILD_EXAMPLES=OFF", "-DOPENTLV_WARNINGS_AS_ERRORS=ON"]


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def ownership(path):
    if path.startswith("bindings/wasm/"):
        return "opentlv-js", path
    if path == "tests/query/wasm_facade.mjs":
        return "opentlv-js", "tests/query/wasm_facade.mjs"
    if path == "docs/development/webassembly.md":
        return "opentlv-js", path
    if path == ".github/workflows/wasm.yml":
        return "opentlv-js", "adapt build/test contract in standalone CI"
    return "Core", None


def inventory():
    files = []
    entries = []
    for line in git("ls-tree", "-r", BASELINE, "--", *INPUTS).decode().splitlines():
        metadata, path = line.split("\t")
        mode, kind, blob = metadata.split()
        if kind != "blob":
            raise ValueError(f"Unexpected Git object: {path}")
        entries.append((mode, blob, path))
    # One Git process avoids per-object startup/Windows filesystem overhead in WSL.
    stream = io.BytesIO(subprocess.check_output(
        ["git", "-C", str(ROOT), "cat-file", "--batch"],
        input="".join(f"{blob}\n" for _, blob, _ in entries).encode()))
    for mode, blob, path in entries:
        actual_blob, kind, size = stream.readline().decode().split()
        if actual_blob != blob or kind != "blob":
            raise ValueError(f"Unexpected batch object: {path}")
        data = stream.read(int(size))
        if len(data) != int(size) or stream.read(1) != b"\n":
            raise ValueError(f"Truncated batch object: {path}")
        owner, destination = ownership(path)
        files.append(dict(path=path, mode=mode, blob=blob, size=len(data),
                          sha256=digest(data), owner=owner, migration_destination=destination))
    header = git("show", f"{BASELINE}:bindings/wasm/src/opentlv_wasm.h").decode()
    declarations = [" ".join(match.split()) for match in re.findall(
        r"^OPENTLV_WASM_API\s+([^;]+);", header, re.M)]
    return dict(schema=1, repository="https://github.com/OpenTLV/opentlv",
                commit=BASELINE, tree=git("rev-parse", f"{BASELINE}^{{tree}}").decode().strip(),
                describe=git("describe", "--tags", "--always", BASELINE).decode().strip(),
                exact_tags=git("tag", "--points-at", BASELINE).decode().splitlines(),
                nearest_tag="0.7.1", nearest_tag_commit="e12a52ff69a64468eecab084795a0530c273efea",
                emscripten="4.0.10", node_major=22, cmake_flags=FLAGS,
                outputs=list(OUTPUTS), bridge_declarations=declarations, files=files)


def verify():
    expected = json.loads((RECORDS / "inventory.json").read_text(encoding="utf-8"))
    if expected != inventory():
        raise ValueError("Immutable source inventory differs from the baseline Git objects")
    print(f"Verified {len(expected['files'])} source blobs and "
          f"{len(expected['bridge_declarations'])} bridge declarations at {BASELINE}")


def reproduce(directory):
    directory = directory.resolve()
    # Never reuse an output directory: stale artifacts cannot become evidence.
    directory.mkdir(parents=True, exist_ok=False)
    steps = []

    def run(name, command, cwd=directory):
        result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, check=False)
        log = directory / f"{name}.log"
        log.write_bytes(result.stdout)
        steps.append(dict(name=name, command=command, exit_code=result.returncode,
                          log=log.name, sha256=digest(result.stdout)))
        write_json(directory / "steps.json", steps)
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {log}")
        print(f"PASS {name}", flush=True)
        return result.stdout.decode()

    emcc = run("emscripten-version", ["emcc", "--version"])
    node = run("node-version", ["node", "--version"]).strip()
    emcc_version = next((line for line in emcc.splitlines() if line.startswith("emcc (")), "")
    if not re.search(r"\) 4\.0\.10(?: |$)", emcc_version) or not node.startswith("v22."):
        raise ValueError("Baseline requires Emscripten 4.0.10 and Node 22")
    run("cmake-version", ["cmake", "--version"])
    run("ninja-version", ["ninja", "--version"])
    source = directory / "source"
    run("clone", ["git", "clone", "--no-hardlinks", str(ROOT), str(source)])
    run("checkout", ["git", "checkout", "--detach", BASELINE], source)
    builds = []
    for name in ("build-wasm", "build-wasm-repeat"):
        build = directory / name
        run(f"{name}-configure", ["emcmake", "cmake", "-S", str(source), "-B", str(build), *FLAGS])
        run(f"{name}-build", ["cmake", "--build", str(build), "--target", "opentlv-wasm", "--parallel"])
        builds.append(build)
    build = builds[0]
    dist = build / "bindings/wasm/dist"
    run("smoke-and-example", ["ctest", "--test-dir", str(build), "-L", "wasm",
                              "--output-on-failure", "--no-tests=error"])
    fixtures = build / "query-fixtures.json"
    run("query-fixtures", ["python3", "tests/query/facade_fixtures.py", "--output", str(fixtures)], source)
    run("query-facade", ["node", "tests/query/wasm_facade.mjs", str(dist), str(fixtures)], source)
    run("abi-build", ["emcc", "-std=c99", "-DTLV_STATIC_DEFINE", "-I", "tlv/include",
                      "-I", str(build / "generated/include"), "tests/query/abi.c",
                      "-o", str(build / "query-abi.js")], source)
    abi = run("abi-observed", ["node", str(build / "query-abi.js")])
    (build / "query-abi.json").write_text(abi, encoding="utf-8")
    run("abi-check", ["python3", "tests/query/check_abi.py", "--input", str(build / "query-abi.json")], source)
    snapshot = run("exports", ["node", str(RECORDS / "exports.mjs"), str(dist)])
    observed = json.loads(snapshot)
    declared = {re.search(r"(opentlv_wasm_\w+)\(", item).group(1)
                for item in inventory()["bridge_declarations"]}
    exported = {name.removeprefix("_") for name in observed["bridgeExports"]}
    if not declared <= exported:
        raise ValueError(f"Missing bridge exports: {sorted(declared - exported)}")
    for name in ("opentlv.mjs", "query.mjs"):
        if (dist / name).read_bytes() != (source / "bindings/wasm/js" / name).read_bytes():
            raise ValueError(f"Copied wrapper differs from source: {name}")
    hashes = [{name: digest((item / "bindings/wasm/dist" / name).read_bytes()) for name in OUTPUTS}
              for item in builds]
    if hashes[0] != hashes[1]:
        raise ValueError("Independent builds are not byte-identical")
    status = run("source-status", ["git", "status", "--porcelain", "--untracked-files=no"], source)
    if status.strip():
        raise ValueError("Baseline source changed during validation")
    write_json(directory / "results.json", dict(
        schema=1, commit=BASELINE, environment="local Linux/WSL", node=node,
        emscripten=emcc_version, cmake_flags=FLAGS, steps=steps,
        runner_sha256=digest(Path(__file__).read_bytes()),
        export_capture_sha256=digest((RECORDS / "exports.mjs").read_bytes()),
        artifacts=hashes[0], independent_builds_identical=True,
        query_abi=json.loads(abi), exports=json.loads(snapshot),
        limitations=["Not hosted CI or a tag-triggered run", "Browser and Web Worker runtime not executed",
                     "Not standalone build, packaging, publication or release evidence"]))
    print(f"Baseline reproduction passed: {directory / 'results.json'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-inventory", action="store_true")
    parser.add_argument("--run", type=Path, help="new output directory (Linux, emcc 4.0.10, Node 22)")
    args = parser.parse_args()
    if args.write_inventory:
        write_json(RECORDS / "inventory.json", inventory())
    verify()
    if args.run:
        reproduce(args.run)


if __name__ == "__main__":
    main()
