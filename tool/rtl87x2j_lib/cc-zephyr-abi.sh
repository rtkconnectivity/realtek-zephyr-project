#!/bin/bash
# Copyright (c) 2026 Realtek Semiconductor Corp.
# SPDX-License-Identifier: Apache-2.0
#
# CMake compiler launcher for the rtl87x2j lib build: runs the compiler command
# line given as arguments with the build attributes Zephyr links with.
#
# The SDK builds RTL87X2J with -fshort-wchar and without -mfp16-format=ieee,
# so every Zephyr link of the libs reports "uses 2-byte wchar_t yet the output
# is to use 4-byte wchar_t". Drop -fshort-wchar and force -mfp16-format=ieee
# here instead of patching the SDK.
#
# gen_provenance.py runs it with "echo" as the compiler to record the flags
# that were really used.
args=()
for a in "$@"; do
	case $a in
	-fshort-wchar | -mfp16-format=*) ;;
	*) args+=("$a") ;;
	esac
done
exec "${args[@]}" -mfp16-format=ieee
