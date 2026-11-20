#!/bin/sh
# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Build-time helper: prepare a Qualcomm "base" for the QDL flash bundles, i.e.
# the boot firmware and CDT of a Qualcomm reference board, into
# $OUT_DIR/base/<base>/, and its default UFS partition layout (qcom-ptool
# format) into $OUT_DIR/layouts/<base>.conf.
#
# Usage: prepare-base.sh <base> <soc> <cdt file>
#   base:     qcom-ptool platform name, e.g. iq-8275-evk
#   soc:      boot binaries SoC name, e.g. qcs8300
#   cdt file: CDT binary inside the CDT zip for this board
#
# Environment:
#   DL_DIR        directory with the downloaded zip files
#   PTOOL_DIR     qcom-ptool source tree
#   OUT_DIR       output directory
#   BOOTBIN_VER   boot binaries version, e.g. 00142
#
set -e

BASE="$1"
SOC="$2"
CDT_FILE="$3"
if [ -z "$BASE" ] || [ -z "$SOC" ] || [ -z "$CDT_FILE" ]; then
    echo "Usage: $0 <base> <soc> <cdt file>" >&2
    exit 1
fi

SOC_UPPER=$(echo "$SOC" | tr '[:lower:]' '[:upper:]')
BOOTBIN="${SOC_UPPER}_bootbinaries"
DEST="$OUT_DIR/base/$BASE"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$DEST" "$OUT_DIR/layouts"

### Boot firmware (same file selection as meta-qcom firmware-qcom-boot-common.inc
### and image_types_qcom.bbclass)
unzip -q "$DL_DIR/${BOOTBIN}_${BOOTBIN_VER}.zip" -d "$WORK/fw"
unzip -q "$DL_DIR/${BOOTBIN}_immutable_${BOOTBIN_VER}.zip" -d "$WORK/fw"
for dir in "$WORK/fw/${BOOTBIN}" "$WORK/fw/${BOOTBIN}_immutable"; do
    find "$dir" -maxdepth 1 -type f \( \
        -name '*.bin' -o -name '*.elf' -o -name '*.fv' -o -name '*.mbn' \
        -o -name '*.melf' -o -name '*.lzma' -o -name '*.xz' \) \
        ! -name 'gpt_*' ! -name 'zeros_*' ! -name 'xbl_config*.elf' \
        -exec install -m 0644 {} "$DEST/" \;
done
# EVE runs on KVM (EL2 available to Linux), so use the non-Gunyah XBL config,
# as meta-qcom does for machines with the "kvm" feature
install -m 0644 "$WORK/fw/${BOOTBIN}/xbl_config_kvm.elf" "$DEST/xbl_config.elf"
install -m 0644 "$WORK/fw/${BOOTBIN}/LICENSE.qcom-2" "$DEST/"

# SAIL (safety island) NOR flash, programmed separately with
# "qdl --storage spinor" (see README.md)
if [ -d "$WORK/fw/${BOOTBIN}/sail_nor" ]; then
    mkdir -p "$DEST/sail_nor"
    for f in gpt_backup0.bin gpt_main0.bin prog_firehose_ddr.elf patch0.xml \
             rawprogram0.xml sailhyp.elf sailsw1.elf; do
        install -m 0644 "$WORK/fw/${BOOTBIN}/sail_nor/$f" "$DEST/sail_nor/"
    done
fi

### CDT
unzip -q "$DL_DIR/cdt-${BASE}.zip" -d "$WORK/cdt"
install -m 0644 "$WORK/cdt/$CDT_FILE" "$DEST/cdt.bin"

### Default partition layout of the base (LUN 0 is replaced by EVE's)
install -m 0644 "$PTOOL_DIR/platforms/$BASE/ufs/partitions.conf" "$OUT_DIR/layouts/$BASE.conf"

echo "$BASE: $(find "$DEST" -type f | wc -l) files"
