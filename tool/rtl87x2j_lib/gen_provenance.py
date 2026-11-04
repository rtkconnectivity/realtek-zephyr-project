#!/usr/bin/env python3
# Copyright (c) 2026 Realtek Semiconductor Corp.
# SPDX-License-Identifier: Apache-2.0
"""Write the provenance record of the rtl87x2j prebuilt libraries.

lib_version is the version of the three libraries as a set, independent of
the Zephyr and SDK versions; bump it whenever the archives change.

For every library it records the archive sha256, the defconfig, the compile
flags as passed to the compiler (after cc-zephyr-abi.sh) and the multirepo
projects whose files went into it (taken from the compiler dependency files
of the build), plus the SHA of each of those projects and of the manifest
repository. The tool is recorded by its PR (--tool-pr): the branch is rebased
and never merged, so its commit SHA would not last. When any of them, or this
tool directory, has uncommitted
changes, a used source file is untracked or a used project has commits that
are not in its manifest-rev, the archive cannot be rebuilt from the SHAs; the
record is still written (a temporary lib built from local changes is a normal
use), with a warning, clean_sources: false and the list under dirty.

Outputs:
  <out>/PROVENANCE.yml       names, SHAs, flags only; no URLs, safe to commit
  <out>/SHA256SUMS           checksum of each archive (sha256sum -c format)
  <frozen>                   "west manifest --freeze" of the multirepo; has
                             internal URLs, do NOT commit it to a public repo

usage:
  gen_provenance.py --lib-version X.Y.Z --mr DIR --out DIR --frozen FILE
                    --cc GCC --tool-pr URL
                    --lib BUILD_DIR:ARCHIVE:DEFCONFIG[:EXTRA_LINE] ...
"""

import argparse
import datetime
import hashlib
import os
import subprocess
import sys

import yaml

# Written by the lib build itself (version info, build time); excluded from the
# cleanliness check and listed in the record instead.
GENERATED = {
    ("driver", "proj/inc/driver_ver.h"),
    ("kernel", "proj/inc/kernel_ver.h"),
}
TOOL_DIR = os.path.dirname(os.path.realpath(__file__))
LAUNCHER = os.path.join(TOOL_DIR, "cc-zephyr-abi.sh")


def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, check=True, capture_output=True,
                          text=True).stdout.strip()


def git(path, *args):
    return run(["git", "-C", path, *args])


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def dep_files(build_dir):
    """Every file named in the gcc .d files of a cmake build tree."""
    files = set()
    for root, _, names in os.walk(build_dir):
        for n in names:
            if not n.endswith(".obj.d"):
                continue
            with open(os.path.join(root, n)) as f:
                text = f.read().replace("\\\n", " ")
            # first token is the target "foo.obj:"
            files.update(t for t in text.split()[1:] if t.startswith("/"))
    return files


def flags_make(build_dir):
    out = {}
    for root, _, names in os.walk(build_dir):
        if "flags.make" in names and "CMakeFiles" in root:
            with open(os.path.join(root, "flags.make")) as f:
                for line in f:
                    k, sep, v = line.partition(" = ")
                    if sep and k in ("C_FLAGS", "C_DEFINES"):
                        out[k] = " ".join(v.split())
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lib-version", required=True)
    ap.add_argument("--mr", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--frozen", required=True)
    ap.add_argument("--cc", required=True)
    ap.add_argument("--tool-pr", required=True)
    ap.add_argument("--lib", action="append", required=True)
    a = ap.parse_args()

    mr = os.path.realpath(a.mr)
    projects = []  # (name, abs path, revision), deepest path first
    for line in run(["west", "list", "-f", "{name}\t{path}\t{revision}"],
                    cwd=mr).splitlines():
        name, path, rev = line.split("\t")
        projects.append((name, os.path.join(mr, path), rev))
    projects.sort(key=lambda p: len(p[1]), reverse=True)

    def owner(f):
        for p in projects:
            if f.startswith(p[1] + "/"):
                return p
        return None

    used = {}  # project name -> set of repo-relative files
    libs = {}
    for spec in a.lib:
        build_dir, archive, defconfig, *extra = spec.split(":")
        names = set()
        for f in dep_files(build_dir):
            p = owner(os.path.realpath(f))
            if p is None:
                continue  # toolchain headers
            names.add(p[0])
            used.setdefault(p[0], set()).add(os.path.relpath(os.path.realpath(f), p[1]))
        # The cmake/Kconfig build system lives in the sdk project.
        names.add("sdk")
        used.setdefault("sdk", set())
        fl = flags_make(build_dir)
        entry = {
            "sha256": sha256(archive),
            "defconfig": os.path.relpath(os.path.realpath(defconfig), mr),
        }
        if extra:
            entry["defconfig_append"] = extra
        # What the compiler really got: the SDK flags rewritten by the launcher.
        entry["cflags"] = run([LAUNCHER, "echo", *fl.get("C_FLAGS", "").split()])
        entry["defines"] = fl.get("C_DEFINES", "")
        entry["projects"] = sorted(names)
        libs[os.path.basename(archive)] = entry

    # Cleanliness: tracked changes anywhere in a used project (build files are
    # not in the .d lists), untracked files among the used sources, and local
    # commits that a frozen manifest could not fetch.
    by_name = {p[0]: p for p in projects}
    dirty = []
    for name in sorted(used):
        path = by_name[name][1]
        changed = set(git(path, "diff", "--name-only", "HEAD").splitlines())
        changed -= {f for (n, f) in GENERATED if n == name}
        files = sorted(used[name])
        tracked = set()
        for i in range(0, len(files), 200):
            tracked.update(git(path, "ls-files", "--", *files[i:i + 200]).splitlines())
        untracked = set(files) - tracked - {f for (n, f) in GENERATED if n == name}
        for f in sorted(changed):
            dirty.append(f"{name}: modified {f}")
        for f in sorted(untracked):
            dirty.append(f"{name}: untracked {f}")
        local = git(path, "rev-list", "--count", "refs/heads/manifest-rev..HEAD")
        if local != "0":
            dirty.append(f"{name}: {local} commit(s) not in manifest-rev")

    mpath = run(["west", "config", "manifest.path"], cwd=mr)
    mdir = os.path.join(mr, mpath)
    if git(mdir, "status", "--porcelain", "--untracked-files=no"):
        dirty.append(f"manifest: uncommitted changes in {mpath}")

    tool_root = git(TOOL_DIR, "rev-parse", "--show-toplevel")
    tool_path = os.path.relpath(TOOL_DIR, tool_root)
    if git(tool_root, "status", "--porcelain", "--", tool_path):
        dirty.append(f"realtek-zephyr-project: uncommitted changes in {tool_path}")

    if dirty:
        print("WARNING: sources used by the build are not committed, the "
              "libraries cannot be rebuilt from the SHAs:", file=sys.stderr)
        for d in dirty:
            print("  " + d, file=sys.stderr)

    sources = {}
    for name in sorted(used):
        _, path, rev = by_name[name]
        head = git(path, "rev-parse", "HEAD")
        sources[name] = {
            "revision": rev,
            "commit": head,
            "describe": git(path, "describe", "--tags", "--always", head),
        }

    rec = {
        "lib_version": a.lib_version,
        "built_at": datetime.datetime.now(datetime.timezone.utc)
        .strftime("%Y-%m-%dT%H:%M:%SZ"),
        "clean_sources": not dirty,
        "compiler": run([a.cc, "--version"]).splitlines()[0],
        "build_tool": {
            "repo": "realtek-zephyr-project",
            "path": tool_path,
            "pr": a.tool_pr,
        },
        "manifest": {
            "file": run(["west", "config", "manifest.file"], cwd=mr),
            "commit": git(mdir, "rev-parse", "HEAD"),
        },
        "generated_files": sorted(f"{n}/{f}" for n, f in GENERATED),
        "libraries": libs,
        "sources": sources,
    }
    if dirty:
        rec["dirty"] = dirty

    with open(os.path.join(a.out, "SHA256SUMS"), "w") as f:
        for n, e in libs.items():
            f.write(f"{e['sha256']}  {n}\n")

    with open(os.path.join(a.out, "PROVENANCE.yml"), "w") as f:
        f.write("# Generated by realtek-zephyr-project "
                f"{tool_path}/gen_provenance.py. Do not edit by hand.\n")
        if dirty:
            f.write("# Built from sources with local changes (see dirty); "
                    "cannot be rebuilt from the SHAs.\n")
        else:
            f.write("# The sources are unmodified.\n")
        f.write("# build_tool/cc-zephyr-abi.sh rewrites the ABI flags "
                "(see cflags).\n")
        yaml.safe_dump(rec, f, sort_keys=False, width=1000)

    frozen = run(["west", "manifest", "--freeze"], cwd=mr)
    os.makedirs(os.path.dirname(os.path.realpath(a.frozen)), exist_ok=True)
    with open(a.frozen, "w") as f:
        f.write(frozen + "\n")

    print(f"wrote {a.out}/PROVENANCE.yml and SHA256SUMS (no internal URLs)")
    print(f"wrote {a.frozen} (internal URLs, do not commit)")


if __name__ == "__main__":
    main()
