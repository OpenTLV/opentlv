#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

"""Verify the immutable Go migration inventory and reproduce its Linux baseline."""

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BASELINE = "cb3caed5b09706ce11ecfae839397fd4f99d0025"
RECORDS = ROOT / "docs/development/go-migration"
MODULE = "github.com/marekcingel/OpenTLV/bindings/go"


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def inventory():
    paths = set(git("ls-tree", "-r", "--name-only", BASELINE,
                    "bindings/go").decode().splitlines())
    references = git("grep", "-l", "-E",
                     "bindings/go|github.com/marekcingel/OpenTLV/bindings/go",
                     BASELINE, "--", ".", ":(exclude)CHANGELOG.md").decode()
    paths.update(line.split(":", 1)[1] for line in references.splitlines())
    paths.update(("LICENSE", "docs/guides/go.md", "docs/development/go.md",
                  "tests/query/corpus.json", "tests/query/run.py",
                  "tests/query/reference.py", "tests/query/facade_fixtures.py",
                  "tests/query/minimize.py", "tests/property/corpus.py",
                  "scripts/generate_diagnostic_vocabulary.py",
                  "scripts/test_diagnostic_vocabulary.py"))
    objects = {}
    for line in git("ls-tree", "-r", BASELINE).decode().splitlines():
        metadata, path = line.split("\t", 1)
        if path in paths:
            objects[path] = metadata.split()[2]
    batch = subprocess.check_output(
        ["git", "-C", str(ROOT), "cat-file", "--batch"],
        input="".join(objects[path] + "\n" for path in sorted(paths)).encode())
    offset = 0
    files = []
    for path in sorted(paths):
        end = batch.index(b"\n", offset)
        size = int(batch[offset:end].split()[2])
        data = batch[end + 1:end + 1 + size]
        offset = end + size + 2
        if path.startswith("bindings/go/"):
            owner = "Go"
            if path.endswith("_test.go"):
                kind = "test"
            elif "/examples/" in path:
                kind = "example"
            elif path.endswith(".md"):
                kind = "documentation"
            elif path.endswith("go.mod"):
                kind = "module"
            else:
                kind = "implementation"
        elif path in ("docs/guides/go.md", "docs/development/go.md"):
            owner, kind = "Go", "documentation"
        else:
            owner, kind = "Core", "shared input or integration reference"
        files.append({"path": path, "owner": owner, "kind": kind,
                      "git_blob": objects[path],
                      "sha256": sha256(data), "bytes": len(data)})
    return {"schema": 1, "repository": "https://github.com/OpenTLV/opentlv",
            "commit": BASELINE, "module": MODULE, "go_directive": "1.22",
            "binding_release_version": None, "files": files}


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def verify():
    expected = json.loads((RECORDS / "inventory.json").read_text(encoding="utf-8"))
    if expected != inventory():
        raise SystemExit("Go migration inventory differs from the pinned Git objects")
    print(f"Verified {len(expected['files'])} files at {BASELINE}", flush=True)


def run_baseline(output):
    if platform.system() != "Linux":
        raise SystemExit("The recorded baseline runner requires Linux (WSL is supported)")
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"schema": 1, "core_commit": BASELINE, "module": MODULE,
              "inventory_sha256": sha256((RECORDS / "inventory.json").read_bytes()),
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "platform": platform.platform(), "checks": [], "status": "running"}
    env = os.environ.copy()
    for key in ("CGO_CFLAGS", "CGO_CXXFLAGS", "CGO_CPPFLAGS", "CGO_LDFLAGS",
                "GOEXPERIMENT", "GOOS", "GOARCH"):
        env.pop(key, None)
    env.update(CGO_ENABLED="1", CC="gcc", GOENV="off", GOWORK="off",
               GOFLAGS="", GOTOOLCHAIN="local")

    def run(name, command, cwd=output, extra=None, go_test=False, keep_output=False):
        current_env = dict(env, **(extra or {}))
        result = subprocess.run(command, cwd=cwd, env=current_env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log = name + ".log"
        (output / log).write_bytes(result.stdout)
        text = result.stdout.decode("utf-8", errors="replace")
        check = {"name": name, "command": command, "cwd": str(cwd),
                 "exit_code": result.returncode, "log": log,
                 "log_sha256": sha256(result.stdout)}
        if go_test:
            events = [json.loads(line) for line in text.splitlines() if line.startswith("{")]
            counts = Counter(event["Action"] for event in events if "Test" in event
                             and event["Action"] in ("pass", "skip", "fail"))
            check["test_events"] = dict(counts)
            check["skipped"] = [event["Package"] + "/" + event["Test"] for event in events
                                if "Test" in event and event["Action"] == "skip"]
            check["packages_passed"] = [event["Package"] for event in events
                                        if "Test" not in event and event["Action"] == "pass"]
        if keep_output:
            check["output"] = text
        report["checks"].append(check)
        write_json(output / "results.json", report)
        print(f"{name}: exit {result.returncode}", flush=True)
        if result.returncode:
            print(text[-8000:], flush=True)
            raise RuntimeError(f"{name} failed; see {output / log}")
        if name.endswith("-gofmt") and text.strip():
            raise RuntimeError(f"{name}: formatting differs")

    try:
        for name, command in (("go-version", ["go", "version"]),
                              ("go-env", ["go", "env", "-json", "GOOS", "GOARCH", "CGO_ENABLED", "CC"]),
                              ("gcc-version", ["gcc", "--version"]),
                              ("cmake-version", ["cmake", "--version"]),
                              ("os-release", ["cat", "/etc/os-release"])):
            run(name, command, keep_output=True)
        source = output / "source"
        run("clone", ["git", "clone", "--shared", "--no-checkout", str(ROOT), str(source)])
        run("checkout", ["git", "checkout", "--detach", BASELINE], source)
        run("source-identity", ["git", "rev-parse", "HEAD"], source, keep_output=True)
        run("module-tags", ["git", "tag", "--merged", BASELINE, "-l", "bindings/go/*"],
            source, keep_output=True)
        for config in ("default-static", "minimal-static", "default-shared"):
            build = output / config
            shared = config.endswith("shared")
            minimal = config.startswith("minimal")
            command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                       "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=gcc",
                       "-DOPENTLV_BUILD_SHARED_LIBS=" + ("ON" if shared else "OFF"),
                       "-DOPENTLV_BUILD_TESTS=OFF", "-DOPENTLV_BUILD_EXAMPLES=OFF",
                       "-DOPENTLV_BUILD_CXX=OFF"]
            if minimal:
                command += ["-DOPENTLV_" + option + "=OFF" for option in
                            ("FORMAT_ASN1", "BLUETOOTH", "LLDP", "DHCP", "NFC", "DOCUMENT")]
            run(config + "-configure", command)
            run(config + "-build", ["cmake", "--build", str(build), "--target", "tlv", "--parallel", "2"])
            flags = {"CGO_CPPFLAGS": shlex.join([f"-I{source}/tlv/include",
                                                f"-I{build}/generated/include"]
                                               + ([] if shared else ["-DTLV_STATIC_DEFINE"])),
                     "CGO_LDFLAGS": shlex.join([f"-L{build}/tlv"]),
                     "LD_LIBRARY_PATH": str(build / "tlv"),
                     "GOCACHE": str(output / (config + "-go-cache"))}
            report.setdefault("configurations", {})[config] = flags
            module = source / "bindings/go"
            run(config + "-gofmt", ["gofmt", "-l", "."], module, flags)
            run(config + "-vet", ["go", "vet", "./..."], module, flags)
            for mode in ("test", "race", "cgocheck2"):
                extra = dict(flags)
                command = ["go", "test", "-json", "-count=1"]
                if mode == "race":
                    command.append("-race")
                if mode == "cgocheck2":
                    extra["GOEXPERIMENT"] = "cgocheck2"
                run(config + "-" + mode, command + ["./..."], module, extra, go_test=True)
            examples = ["version", "quick_start", "reader", "writer", "errors", "codec"]
            if not minimal:
                examples += ["document", "query"]
            for example in examples:
                run(config + "-" + example, ["go", "run", "./examples/" + example],
                    module, flags, keep_output=True)
            if not minimal:
                executable = build / "go-query-conformance"
                run(config + "-query-build", ["go", "build", "-o", str(executable),
                                              "./examples/query_conformance"], module, flags)
                run(config + "-query-corpus", ["python3", str(source / "tests/query/run.py"),
                                               "--native", str(executable), "--property-cases", "0"],
                    module, flags, keep_output=True)
        run("source-unchanged", ["git", "diff", "--exit-code", BASELINE], source)
        report["status"] = "passed"
    except (OSError, RuntimeError) as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(output / "results.json", report)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-inventory", action="store_true")
    parser.add_argument("--run", type=Path, metavar="NEW_OUTPUT_DIRECTORY")
    args = parser.parse_args()
    if args.write_inventory:
        write_json(RECORDS / "inventory.json", inventory())
    verify()
    if args.run:
        run_baseline(args.run)


if __name__ == "__main__":
    main()
