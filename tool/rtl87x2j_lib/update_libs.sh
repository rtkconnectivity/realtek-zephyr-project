#!/bin/bash
# Copyright (c) 2026 Realtek Semiconductor Corp.
# SPDX-License-Identifier: Apache-2.0
#
# Rebuild the rtl87x2j prebuilt libs (libdriver / libkernel / libusb) from the
# rtl87x2j multirepo. They are kept in lib/rtl87x2j/ of realtek-zephyr-project
# with a provenance record (PROVENANCE.yml + SHA256SUMS) and copied to
# bee/ld/rtl87x2j/ of hal_realtek. Two Zephyr apps are built in their own
# build/ with MCUboot, ready for "west flash", and their warnings and errors
# are shown; then the libs are committed in realtek-zephyr-project and in
# hal_realtek, unless an app fails to build.
#
# The libs are built with the SDK's own cmake, with cc-zephyr-abi.sh as
# compiler launcher so that the objects carry the wchar_t / fp16 build
# attributes Zephyr links with; nothing is patched in the multirepo.
#
# Local changes in the multirepo are allowed (e.g. a temporary lib to try out
# a fix): they only give a warning, and PROVENANCE.yml records
# clean_sources: false with the list of them.
#
# usage: update_libs.sh [--lib-version X.Y.Z] [--skip-update] [--no-commit]
#        update_libs.sh --local
#        update_libs.sh --drop
#   --lib-version  version of the three libs as a set, recorded as lib_version
#                  in PROVENANCE.yml; independent of the Zephyr and SDK
#                  versions, must differ from the committed one
#                  (default: the committed one with the patch number bumped,
#                  0.1.0 if there is none)
#   --skip-update  do not run `west update --rebase` (reuse current multirepo)
#   --no-commit    leave the libs uncommitted
#   --local        temporary libs from the multirepo as it is, local changes
#                  included: no update, no commit, lib_version <current>+local;
#                  may be rerun over the libs of a previous --local run
#   --drop         restore the libs in both repositories to HEAD and exit
#
# Environment: MR (multirepo workspace), ZW (Zephyr west workspace), HAL
# (hal_realtek in ZW), GCC_BIN (Arm GNU toolchain bin dir), WORK (scratch dir).
set -euo pipefail

TOOLS=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MR=${MR:-$HOME/zephyr/multi_repo_bee5}
ZW=${ZW:-$(cd "$TOOLS" && west topdir)}
HAL=${HAL:-$(cd "$ZW" && west list -f '{abspath}' hal_realtek)}
GCC_BIN=${GCC_BIN:-$HOME/.rtk_bt_soc_tools/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin}
WORK=${WORK:-$HOME/.cache/rtl87x2j-lib-update}
LAUNCHER=$TOOLS/cc-zephyr-abi.sh
BOARD=rtl87x2j_evb/rtl8762jth
APPS=(zephyr/tests/boards/realtek/bee/bee_pm_device zephyr/samples/subsys/usb/hid-mouse)
TOOL_ROOT=$(git -C "$TOOLS" rev-parse --show-toplevel)
TOOL_PATH=${TOOLS#$TOOL_ROOT/}
# The PR that holds this tool. Its branch is rebased and never merged, so the
# commit SHAs do not last; the PR does.
TOOL_PR=https://github.com/rtkconnectivity/realtek-zephyr-project/pull/51
# The libs and their provenance in realtek-zephyr-project, the libs in hal.
REPO_LIB=lib/rtl87x2j
HAL_LIB=bee/ld/rtl87x2j
LIB_FILES=(libdriver.a libkernel.a librtl87x2j_usb.a)
REPO_OUTPUTS=("${LIB_FILES[@]/#/$REPO_LIB/}" "$REPO_LIB/PROVENANCE.yml" "$REPO_LIB/SHA256SUMS")
HAL_OUTPUTS=("${LIB_FILES[@]/#/$HAL_LIB/}")

SKIP_UPDATE=0
NO_COMMIT=0
LOCAL=0
DROP=0
LIB_VERSION=
while [ $# -gt 0 ]; do
	case $1 in
	--lib-version) LIB_VERSION=${2:-}; shift ;;
	--skip-update) SKIP_UPDATE=1 ;;
	--no-commit) NO_COMMIT=1 ;;
	--local) LOCAL=1 SKIP_UPDATE=1 NO_COMMIT=1 ;;
	--drop) DROP=1 ;;
	*) echo "unknown option: $1" >&2; exit 2 ;;
	esac
	shift
done
if [ $LOCAL = 1 ] && [ -n "$LIB_VERSION" ]; then
	echo "--local takes no --lib-version" >&2; exit 2
fi
[ -z "$LIB_VERSION" ] || [[ $LIB_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
	{ echo "--lib-version must be X.Y.Z" >&2; exit 2; }

SDK=$MR/realtek
# name | repo dir | defconfig (relative to repo) | lib built in tree | hal file name
LIBS=(
	"driver|$SDK/driver|proj/rtl87x2j/defconfig.driver.RTL87x2J.bank0|lib/rtl87x2j/gcc/libdriver.a|libdriver.a"
	"kernel|$SDK/kernel|proj/proj/rtl87x2j/defconfig.kernel.RTL87x2J|lib/rtl87x2j/gcc/libkernel.a|libkernel.a"
	"usb|$SDK/subsys/usb|proj/rtl87x2j/gcc/defconfig.usb.RTL87x2J|lib/rtl87x2j/gcc/libusb.a|librtl87x2j_usb.a"
)

die() { echo "ERROR: $*" >&2; exit 1; }
step() { echo; echo "==== $*"; }

# Put the given files of a repository back to HEAD.
restore_head() { # repo file...
	local repo=$1 f
	shift
	for f in "$@"; do
		if git -C "$repo" cat-file -e "HEAD:$f" 2>/dev/null; then
			git -C "$repo" checkout -q HEAD -- "$f"
		else
			rm -f "$repo/$f"
		fi
	done
}
# Whether any of the given files differs from HEAD (*.a are gitignored).
changed() { # repo file...
	local repo=$1
	shift
	[ -n "$(git -C "$repo" status --porcelain --ignored -- "$@")" ]
}
if [ $DROP = 1 ]; then
	restore_head "$TOOL_ROOT" "${REPO_OUTPUTS[@]}"
	restore_head "$HAL" "${HAL_OUTPUTS[@]}"
	rmdir -p "$TOOL_ROOT/$REPO_LIB" 2>/dev/null || true
	echo "restored $TOOL_ROOT/$REPO_LIB and $HAL/$HAL_LIB to HEAD"
	exit 0
fi

mkdir -p "$WORK"
export PYTHONPATH=$ZW/zephyr/scripts/kconfig${PYTHONPATH:+:$PYTHONPATH}
[ -x "$GCC_BIN/arm-none-eabi-gcc" ] || die "toolchain not found: $GCC_BIN"
python3 -c 'import yaml' 2>/dev/null || die "python3 yaml module (PyYAML) is required"

step "precheck"
for r in "$SDK" "$SDK/driver" "$SDK/kernel" "$SDK/subsys/usb"; do
	git -C "$r" diff --quiet HEAD ||
		echo "WARNING: $r has uncommitted tracked changes, the libs will not be" \
			"reproducible from the SHAs" >&2
done
lib_version() { python3 -c 'import sys, yaml; print((yaml.safe_load(sys.stdin) or {}).get("lib_version", ""))'; }
OLD_VERSION=$({ git -C "$TOOL_ROOT" show "HEAD:$REPO_LIB/PROVENANCE.yml" 2>/dev/null || true; } | lib_version)
if changed "$TOOL_ROOT" "${REPO_OUTPUTS[@]}" || changed "$HAL" "${HAL_OUTPUTS[@]}"; then
	# A --local run may replace the temporary libs of the previous one.
	cur=$({ cat "$TOOL_ROOT/$REPO_LIB/PROVENANCE.yml" 2>/dev/null || true; } | lib_version)
	[ $LOCAL = 1 ] && [[ $cur == *+local ]] ||
		die "uncommitted libs in $TOOL_ROOT/$REPO_LIB or $HAL/$HAL_LIB" \
			"(drop them with --drop)"
fi
if [ $LOCAL = 1 ]; then
	LIB_VERSION=${OLD_VERSION:-0.0.0}+local
elif [ -z "$LIB_VERSION" ] && [ -z "$OLD_VERSION" ]; then
	LIB_VERSION=0.1.0
elif [ -z "$LIB_VERSION" ]; then
	[[ $OLD_VERSION =~ ^([0-9]+\.[0-9]+)\.([0-9]+)$ ]] ||
		die "committed lib_version $OLD_VERSION is not X.Y.Z, give --lib-version"
	LIB_VERSION=${BASH_REMATCH[1]}.$((BASH_REMATCH[2] + 1))
fi
[ "$LIB_VERSION" != "$OLD_VERSION" ] ||
	die "lib_version $LIB_VERSION is already committed, bump it"
echo "lib_version: ${OLD_VERSION:-none} -> $LIB_VERSION"

if [ $SKIP_UPDATE = 0 ]; then
	step "west update --rebase ($MR)"
	(cd "$MR" && west update --rebase) ||
		die "west update --rebase failed; resolve it, then rerun with --skip-update"
fi

# Files a lib build writes into the multirepo trees besides the lib itself.
GENERATED=("$SDK/driver/proj/inc/driver_ver.h" "$SDK/kernel/proj/inc/kernel_ver.h")
# Revert only those (and the libs), so that local source changes survive.
restore_trees() {
	local f
	for f in "${GENERATED[@]}"; do
		git -C "$(dirname "$f")" checkout -q HEAD -- "$f"
	done
	for e in "${LIBS[@]}"; do
		IFS='|' read -r _ repo _ lib _ <<<"$e"
		if git -C "$repo" ls-files --error-unmatch "$lib" >/dev/null 2>&1; then
			git -C "$repo" checkout -q HEAD -- "$lib"
		else
			rm -f "$repo/$lib"
		fi
	done
}
trap restore_trees EXIT

step "build libs"
for e in "${LIBS[@]}"; do
	IFS='|' read -r name repo cfg lib _ <<<"$e"
	cfgfile=$repo/$cfg
	if [ "$name" = driver ]; then
		# REALTEK_DRIVER is promptless, CONFIG_REALTEK_DRIVER=y alone is
		# ignored; any driver option selects it and enables the lib target.
		cp "$cfgfile" "$WORK/defconfig.driver"
		echo "CONFIG_REALTEK_DRIVER_ADC=y" >>"$WORK/defconfig.driver"
		cfgfile=$WORK/defconfig.driver
	fi
	rm -rf "$WORK/lib_$name" "${repo:?}/$lib"
	echo "-- $name"
	if ! { cmake -G "Unix Makefiles" -D kconfig_path="$cfgfile" -D compile_lib_only=ON \
		-D COMPILER_TYPE=gcc -D COMPILER_DIR="$GCC_BIN" \
		-D CMAKE_C_COMPILER_LAUNCHER="$LAUNCHER" \
		-D CMAKE_CXX_COMPILER_LAUNCHER="$LAUNCHER" \
		-B "$WORK/lib_$name" -S "$SDK" &&
		cmake --build "$WORK/lib_$name" -j"$(nproc)"; } >"$WORK/lib_$name.log" 2>&1; then
		die "$name lib build failed, see $WORK/lib_$name.log"
	fi
	[ -f "$repo/$lib" ] || die "$name: $repo/$lib not produced, see $WORK/lib_$name.log"
	attrs=$("$GCC_BIN/arm-none-eabi-readelf" -A "$repo/$lib")
	grep -q 'Tag_ABI_PCS_wchar_t: 2' <<<"$attrs" &&
		die "$name: still has 2-byte wchar_t objects"
	grep -q 'Tag_ABI_FP_16bit_format: IEEE 754' <<<"$attrs" ||
		die "$name: objects are not built with -mfp16-format=ieee"
	cp "$repo/$lib" "$WORK/$name.a"
done
# Libs are saved in $WORK; give the trees back now so that the provenance
# check below does not see the build outputs.
restore_trees

step "store libs in $TOOL_ROOT/$REPO_LIB, copy to $HAL/$HAL_LIB"
mkdir -p "$TOOL_ROOT/$REPO_LIB" "$HAL/$HAL_LIB"
for e in "${LIBS[@]}"; do
	IFS='|' read -r name _ _ _ dst <<<"$e"
	cp "$WORK/$name.a" "$TOOL_ROOT/$REPO_LIB/$dst"
	cp "$WORK/$name.a" "$HAL/$HAL_LIB/$dst"
done

step "provenance"
prov_args=()
for e in "${LIBS[@]}"; do
	IFS='|' read -r name repo cfg _ dst <<<"$e"
	spec="$WORK/lib_$name:$TOOL_ROOT/$REPO_LIB/$dst:$repo/$cfg"
	[ "$name" = driver ] && spec="$spec:CONFIG_REALTEK_DRIVER_ADC=y"
	prov_args+=(--lib "$spec")
done
FROZEN=$WORK/frozen/$LIB_VERSION.yml
python3 "$TOOLS/gen_provenance.py" --lib-version "$LIB_VERSION" --mr "$MR" \
	--out "$TOOL_ROOT/$REPO_LIB" --frozen "$FROZEN" --cc "$GCC_BIN/arm-none-eabi-gcc" \
	--tool-pr "$TOOL_PR" "${prov_args[@]}"
(cd "$TOOL_ROOT/$REPO_LIB" && sha256sum -c --quiet SHA256SUMS) || die "SHA256SUMS check failed"
(cd "$HAL/$HAL_LIB" && sha256sum -c --quiet "$TOOL_ROOT/$REPO_LIB/SHA256SUMS") ||
	die "libs in $HAL/$HAL_LIB differ from SHA256SUMS"

step "build zephyr apps"
failed=()
for app in "${APPS[@]}"; do
	tag=$(basename "$app")
	log=$WORK/zephyr_$tag.log
	echo "-- $app"
	# Build in the app's own build/ with MCUboot, so that "west flash" can be
	# run right there to try the new libs on the board.
	(cd "$ZW/$app" && west build -p always -b "$BOARD" --sysbuild -- \
		-DSB_CONFIG_BOOTLOADER_MCUBOOT=y) >"$log" 2>&1 || failed+=("$app")
	# Show warnings and errors, but do not stop on warnings.
	if grep -nE '(warning|error):' "$log"; then
		echo "   ^ from $log"
	fi
done
[ ${#failed[@]} = 0 ] ||
	die "build failed: ${failed[*]}; libs $LIB_VERSION left uncommitted," \
		"\"$0 --drop\" restores them"

if ! changed "$TOOL_ROOT" "${REPO_OUTPUTS[@]}" && ! changed "$HAL" "${HAL_OUTPUTS[@]}"; then
	echo "libs unchanged, nothing to commit"
	exit 0
fi
if [ $NO_COMMIT = 1 ]; then
	echo "libs $LIB_VERSION left uncommitted in $TOOL_ROOT/$REPO_LIB and $HAL/$HAL_LIB;" \
		"\"$0 --drop\" restores them"
	exit 0
fi

step "commit in realtek-zephyr-project and hal"
mr_rev() { (cd "$MR" && west list -f '{revision}' "$1"); }
src_line() { # project repo-dir
	printf '  %-7s %-19s %-8s %s\n' "$1" "${2#$MR/}" "$(mr_rev "$1")" "$(git -C "$2" rev-parse HEAD)"
}
CLEAN=$(python3 -c 'import sys, yaml; print(yaml.safe_load(open(sys.argv[1]))["clean_sources"])' \
	"$TOOL_ROOT/$REPO_LIB/PROVENANCE.yml")
signoff() { # repo
	echo "Signed-off-by: $(git -C "$1" config user.name) <$(git -C "$1" config user.email)>"
	echo "Co-Authored-By: Claude <noreply@anthropic.com>"
}
version_lines() {
	echo "lib_version ${OLD_VERSION:-none} -> $LIB_VERSION (archive, sha256[:8]):"
	for f in "${LIB_FILES[@]}"; do
		printf '  %-19s %s\n' "$f" "$(sha256sum "$TOOL_ROOT/$REPO_LIB/$f" | cut -c1-8)"
	done
}
commit() { # repo message-file file...
	local repo=$1 msg=$2
	shift 2
	# The commit message must not mention the product family, only the IC.
	grep -qi bee "$msg" && die "commit message mentions 'bee', see $msg"
	git -C "$repo" add -f -- "$@"
	git -C "$repo" commit -q -F "$msg" -- "$@"
	git -C "$repo" log -1 --stat | cat
}

msg=$WORK/commit_msg_repo.txt
{
	echo "lib: rtl87x2j: update driver, kernel and usb libraries to $LIB_VERSION"
	echo
	echo "Rebuild libdriver.a, libkernel.a and librtl87x2j_usb.a from the rtl87x2j"
	echo "multirepo with Arm GNU Toolchain 13.2.Rel1."
	echo
	version_lines
	echo
	echo "Sources (project, path, branch, sha):"
	src_line driver "$SDK/driver"
	src_line kernel "$SDK/kernel"
	src_line usb "$SDK/subsys/usb"
	src_line sdk "$SDK"
	echo
	if [ "$CLEAN" = True ]; then
		echo "The sources are unmodified."
	else
		echo "WARNING: built from sources with local changes, listed under dirty"
		echo "in PROVENANCE.yml; the libs cannot be rebuilt from the SHAs above."
	fi
	echo "To avoid the link warning \"uses 2-byte wchar_t yet the output is to"
	echo "use 4-byte wchar_t\", the compiler runs through"
	echo "$TOOL_PATH/cc-zephyr-abi.sh, which drops -fshort-wchar and adds"
	echo "-mfp16-format=ieee, so the objects carry the same wchar_t and fp16 build"
	echo "attributes as Zephyr."
	echo
	echo "$REPO_LIB/PROVENANCE.yml records lib_version, the archive sha256,"
	echo "defconfig, compile flags and the source projects of each library;"
	echo "SHA256SUMS holds the checksums. Both are written by"
	echo "$TOOL_PATH/update_libs.sh, which did this whole update and copied the"
	echo "libraries to hal_realtek."
	echo
	echo "Tool: $TOOL_PR"
	echo
	signoff "$TOOL_ROOT"
} >"$msg"
commit "$TOOL_ROOT" "$msg" "${REPO_OUTPUTS[@]}"

msg=$WORK/commit_msg_hal.txt
{
	echo "lib: rtl87x2j: update driver, kernel and usb libraries to $LIB_VERSION"
	echo
	echo "Update libdriver.a, libkernel.a and librtl87x2j_usb.a to lib_version"
	echo "$LIB_VERSION of realtek-zephyr-project $REPO_LIB/."
	echo
	version_lines
	echo
	echo "How they were built, from which sources, is recorded in"
	echo "$REPO_LIB/PROVENANCE.yml of lib_version $LIB_VERSION."
	[ "$CLEAN" = True ] || echo "WARNING: they were built from sources with local changes."
	echo
	echo "Tool: $TOOL_PR"
	echo
	signoff "$HAL"
} >"$msg"
commit "$HAL" "$msg" "${HAL_OUTPUTS[@]}"
