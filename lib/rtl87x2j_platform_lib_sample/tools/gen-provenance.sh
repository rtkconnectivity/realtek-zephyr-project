#!/usr/bin/env bash
# Copyright (c) 2026 Realtek Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Generate the provenance record of librtl87x2j_platform_lib.a.
#
# It records which commits of the bee5 workspace and of this repository the
# archive was built from, and refuses to do so when any of them has uncommitted
# changes, because such an archive could not be reproduced from the SHAs.
#
# Outputs (in OUT_DIR):
#   PROVENANCE.yml              SHAs, versions and build environment only.
#                               No internal URLs, safe to commit.
#   bee5-frozen.yml             "west manifest --freeze" of the bee5 workspace.
#                               Contains internal URLs, do NOT commit it to a
#                               public repository. Keep it next to the archive.
#   <archive>.sha256            checksum of the archive
#
# Usage:
#   gen-provenance.sh --lib PATH/librtl87x2j_platform_lib.a \
#                     --lib-version 0.1.0 \
#                     [--bee5 ~/bee5] [--out DIR] [--west PATH] [--cc PATH] \
#                     [--cflags "..."] [--allow-dirty]
#
# --allow-dirty writes the record anyway and marks it "reproducible: false".

set -euo pipefail

BEE5="${BEE5:-$HOME/bee5}"
WEST="${WEST:-west}"
CC="${CC:-arm-zephyr-eabi-gcc}"
CFLAGS_REC=""
LIB=""
LIB_VERSION=""
OUT=""
ALLOW_DIRTY=0

die() { echo "error: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
	case "$1" in
	--lib) LIB="$2"; shift 2 ;;
	--lib-version) LIB_VERSION="$2"; shift 2 ;;
	--bee5) BEE5="$2"; shift 2 ;;
	--out) OUT="$2"; shift 2 ;;
	--west) WEST="$2"; shift 2 ;;
	--cc) CC="$2"; shift 2 ;;
	--cflags) CFLAGS_REC="$2"; shift 2 ;;
	--allow-dirty) ALLOW_DIRTY=1; shift ;;
	-h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
	*) die "unknown option: $1" ;;
	esac
done

[ -n "$LIB" ] || die "--lib is required"
[ -f "$LIB" ] || die "archive not found: $LIB"
[ -n "$LIB_VERSION" ] || die "--lib-version is required"
[ -d "$BEE5/.west" ] || die "not a west workspace: $BEE5"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_PROJECT_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
# Resolve to absolute paths now: parts of this script run from inside the bee5
# workspace, where a relative --lib or --out would point somewhere else.
LIB="$(realpath "$LIB")"
OUT="$(realpath -m "${OUT:-$(dirname "$LIB")}")"
mkdir -p "$OUT"
LIB_NAME="$(basename "$LIB")"

# ---------------------------------------------------------------------------
# 1. Cleanliness check
# ---------------------------------------------------------------------------
DIRTY=()

check_clean() {
	# $1 = display name, $2 = path of a git work tree
	local out
	out="$(git -C "$2" status --porcelain --untracked-files=normal 2>/dev/null)" ||
		die "not a git work tree: $2"
	[ -z "$out" ] || DIRTY+=("$1")
}

# The bee5 workspace: every project, plus the manifest repository itself.
# Project paths come from "west list" so that this stays correct when the
# manifest changes. The list is captured first: reading it through a process
# substitution would hide a failure of "west list" and silently skip the check.
PROJECTS="$(cd "$BEE5" && "$WEST" list -f '{name}'$'\t''{path}')" ||
	die "'west list' failed in $BEE5 (is west available? see --west)"
[ -n "$PROJECTS" ] || die "'west list' returned no projects in $BEE5"

while IFS=$'\t' read -r name path; do
	check_clean "$name" "$BEE5/$path"
done <<< "$PROJECTS"

check_clean "realtek-zephyr-project" "$LIB_PROJECT_ROOT"

REPRODUCIBLE=true
if [ ${#DIRTY[@]} -gt 0 ]; then
	echo "These repositories have uncommitted changes:" >&2
	printf '  - %s\n' "${DIRTY[@]}" >&2
	if [ "$ALLOW_DIRTY" -ne 1 ]; then
		die "refusing to record an archive that cannot be reproduced from SHAs (use --allow-dirty to override)"
	fi
	REPRODUCIBLE=false
fi

# ---------------------------------------------------------------------------
# 2. Collect facts
# ---------------------------------------------------------------------------
yaml_str() { printf '"%s"' "${1//\"/\\\"}"; }

LIB_SHA256="$(sha256sum "$LIB" | awk '{print $1}')"
printf '%s  %s\n' "$LIB_SHA256" "$LIB_NAME" > "$OUT/$LIB_NAME.sha256"

(cd "$BEE5" && "$WEST" manifest --freeze -o "$OUT/bee5-frozen.yml") ||
	die "'west manifest --freeze' failed"

CC_VERSION="$("$CC" --version 2>/dev/null | head -n1 || echo unknown)"

MANIFEST_DIR="$(cd "$BEE5" && "$WEST" config manifest.path)"
MANIFEST_FILE="$(cd "$BEE5" && "$WEST" config manifest.file)"
MANIFEST_SHA="$(git -C "$BEE5/$MANIFEST_DIR" rev-parse HEAD)"
MANIFEST_BRANCH="$(git -C "$BEE5/$MANIFEST_DIR" rev-parse --abbrev-ref HEAD)"

LIB_PROJECT_SHA="$(git -C "$LIB_PROJECT_ROOT" rev-parse HEAD)"
LIB_PROJECT_BRANCH="$(git -C "$LIB_PROJECT_ROOT" rev-parse --abbrev-ref HEAD)"
LIB_PROJECT_PATH="$(realpath --relative-to="$LIB_PROJECT_ROOT" "$SCRIPT_DIR/..")"

# ---------------------------------------------------------------------------
# 3. PROVENANCE.yml  (no internal URLs)
# ---------------------------------------------------------------------------
{
	echo "# Generated by tools/gen-provenance.sh. Do not edit by hand."
	echo "library: $(yaml_str "$LIB_NAME")"
	echo "lib_version: $(yaml_str "$LIB_VERSION")"
	echo "built_at: $(yaml_str "$(date -u +%Y-%m-%dT%H:%M:%SZ)")"
	echo "reproducible: $REPRODUCIBLE"
	echo "archive_sha256: $(yaml_str "$LIB_SHA256")"
	echo
	echo "build:"
	echo "  compiler: $(yaml_str "$CC_VERSION")"
	echo "  cflags: $(yaml_str "$CFLAGS_REC")"
	echo
	echo "lib_project:"
	echo "  name: realtek-zephyr-project"
	echo "  path: $(yaml_str "$LIB_PROJECT_PATH")"
	echo "  branch: $(yaml_str "$LIB_PROJECT_BRANCH")"
	echo "  commit: $(yaml_str "$LIB_PROJECT_SHA")"
	echo
	echo "bee5_manifest:"
	echo "  file: $(yaml_str "$MANIFEST_FILE")"
	echo "  branch: $(yaml_str "$MANIFEST_BRANCH")"
	echo "  commit: $(yaml_str "$MANIFEST_SHA")"
	echo
	echo "sources:"
	# Names and SHAs only. Paths and URLs stay in bee5-frozen.yml.
	while IFS=$'\t' read -r name path; do
		[ "$name" = "manifest" ] && continue
		sha="$(git -C "$BEE5/$path" rev-parse HEAD)"
		desc="$(git -C "$BEE5/$path" describe --tags --always 2>/dev/null || echo "$sha")"
		echo "  $name:"
		echo "    commit: $(yaml_str "$sha")"
		echo "    describe: $(yaml_str "$desc")"
	done <<< "$PROJECTS"
	if [ ${#DIRTY[@]} -gt 0 ]; then
		echo
		echo "dirty:"
		printf '  - %s\n' "${DIRTY[@]}"
	fi
} > "$OUT/PROVENANCE.yml"

echo "Wrote:"
echo "  $OUT/PROVENANCE.yml     (safe to commit, no internal URLs)"
echo "  $OUT/bee5-frozen.yml    (internal URLs, keep next to the archive)"
echo "  $OUT/$LIB_NAME.sha256"
[ "$REPRODUCIBLE" = true ] || echo "WARNING: recorded with uncommitted changes, reproducible: false" >&2
