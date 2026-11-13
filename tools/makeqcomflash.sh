#!/bin/sh
# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Build a QDL flash bundle for Qualcomm UFS devices with pkg/qcom-flash.
#
# Usage:
#
#      ./makeqcomflash.sh <board> <input dir> <output dir> [dtb.bin] [board dir]
#
#   board:      board name, one of pkg/qcom-flash/boards/ (e.g. iq-9075-evk,
#               iq-8275-evk), or the name of a custom board given in board dir
#   input dir:  EVE installer directory (EFI/, boot/, rootfs.img, config.img)
#   output dir: directory to write the flash bundle to (recreated)
#   dtb.bin:    optional multi-DTB FIT VFAT image for the dtb_a/dtb_b
#               partitions ("" to skip)
#   board dir:  optional custom board directory (board.conf, grub.cfg, dtb/,
#               ...) kept outside of EVE's tree, see pkg/qcom-flash/README.md
#
EVE="$(cd "$(dirname "$0")" && pwd)/../"
PATH="$EVE/build-tools/bin:$PATH"

if [ $# -lt 3 ]; then
    echo "Usage: $0 <board> <input dir> <output dir> [dtb.bin] [board dir]" >&2
    exit 1
fi

# shellcheck disable=SC2086
QCOMFLASH_TAG="$(linuxkit pkg ${LINUXKIT_ORG_TARGET} show-tag "$EVE/pkg/qcom-flash")"

BOARD="$1"
SOURCE="$(cd "$2" && pwd)"
mkdir -p "$3"
OUTPUT="$(cd "$3" && pwd)"
DTB_ARG="$4"
BOARD_DIR_ARG="$5"

# Extra docker arguments, built in the positional parameters (all the script
# arguments were saved above). /parts is read-only, so extra files can't be
# mounted inside it.
set --
if [ -n "$DTB_ARG" ]; then
    DTB="$(cd "$(dirname "$DTB_ARG")" && pwd)/$(basename "$DTB_ARG")"
    [ -f "$DTB" ] || { echo "$DTB_ARG not found" >&2; exit 1; }
    set -- "$@" -v "$DTB:/dtb.bin:ro" -e DTB_BIN=/dtb.bin
fi
if [ -n "$BOARD_DIR_ARG" ]; then
    BOARD_DIR="$(cd "$BOARD_DIR_ARG" && pwd)" || { echo "$BOARD_DIR_ARG not found" >&2; exit 1; }
    [ -f "$BOARD_DIR/board.conf" ] || { echo "$BOARD_DIR/board.conf not found" >&2; exit 1; }
    set -- "$@" -v "$BOARD_DIR:/board:ro" -e BOARD_DIR=/board
fi

# Run as the calling user, so the bundle isn't owned by root
docker run --rm --user "$(id -u):$(id -g)" -e DEBUG="$DEBUG" \
    -v "$SOURCE:/parts:ro" "$@" -v "$OUTPUT:/out" \
    "$QCOMFLASH_TAG" "$BOARD" /out
