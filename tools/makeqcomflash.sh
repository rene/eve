#!/bin/sh
# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Build a QDL flash bundle for Qualcomm UFS devices with pkg/qcom-flash.
#
# Usage:
#
#      ./makeqcomflash.sh <board> <input dir> <output dir> [dtb.bin]
#
#   board:      qcom-flash board, e.g. iq-9075-evk or iq-8275-evk
#   input dir:  EVE installer directory (EFI/, boot/, rootfs.img, config.img)
#   output dir: directory to write the flash bundle to (recreated)
#   dtb.bin:    optional multi-DTB FIT VFAT image for the dtb_a/dtb_b partitions
#
EVE="$(cd "$(dirname "$0")" && pwd)/../"
PATH="$EVE/build-tools/bin:$PATH"
# shellcheck disable=SC2086
QCOMFLASH_TAG="$(linuxkit pkg ${LINUXKIT_ORG_TARGET} show-tag "$EVE/pkg/qcom-flash")"

if [ $# -lt 3 ]; then
    echo "Usage: $0 <board> <input dir> <output dir> [dtb.bin]" >&2
    exit 1
fi

BOARD="$1"
SOURCE="$(cd "$2" && pwd)"
mkdir -p "$3"
OUTPUT="$(cd "$3" && pwd)"

set --
if [ -n "$4" ]; then
    DTB="$(cd "$(dirname "$4")" && pwd)/$(basename "$4")"
    [ -f "$DTB" ] || { echo "$DTB not found" >&2; exit 1; }
    # /parts is read-only, so the file can't be mounted inside it
    set -- -v "$DTB:/dtb.bin:ro" -e DTB_BIN=/dtb.bin
fi

# Run as the calling user, so the bundle isn't owned by root
docker run --rm --user "$(id -u):$(id -g)" -e DEBUG="$DEBUG" \
    -v "$SOURCE:/parts:ro" "$@" -v "$OUTPUT:/out" \
    "$QCOMFLASH_TAG" "$BOARD" /out
