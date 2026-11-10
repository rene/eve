#!/bin/sh
# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Build-time helper: prepare the static part of the QDL flash bundle for one
# board (boot firmware, CDT and partition tables) into $OUT_DIR/<board>/.
#
# Usage: prepare-board.sh <board> <soc> <cdt file>
#   board:    qcom-ptool platform name, e.g. iq-8275-evk
#   soc:      boot binaries SoC name, e.g. qcs8300
#   cdt file: CDT binary inside the CDT zip for this board
#
# Environment:
#   DL_DIR        directory with the downloaded zip files
#   PTOOL_DIR     qcom-ptool source tree (patched)
#   LUN0_CONF     EVE LUN 0 layout (qcom-ptool .conf format)
#   OUT_DIR       output directory
#   BOOTBIN_VER   boot binaries version, e.g. 00142
#
set -e

BOARD="$1"
SOC="$2"
CDT_FILE="$3"
if [ -z "$BOARD" ] || [ -z "$SOC" ] || [ -z "$CDT_FILE" ]; then
    echo "Usage: $0 <board> <soc> <cdt file>" >&2
    exit 1
fi

SOC_UPPER=$(echo "$SOC" | tr '[:lower:]' '[:upper:]')
BOOTBIN="${SOC_UPPER}_bootbinaries"
DEST="$OUT_DIR/$BOARD"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$DEST"

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
unzip -q "$DL_DIR/cdt-${BOARD}.zip" -d "$WORK/cdt"
install -m 0644 "$WORK/cdt/$CDT_FILE" "$DEST/cdt.bin"

### Partition tables: Qualcomm layout for LUNs 1-5, EVE layout for LUN 0
PCONF="$PTOOL_DIR/platforms/$BOARD/ufs/partitions.conf"
mkdir -p "$WORK/gpt"
{
    grep -v -- '--lun=0 ' "$PCONF"
    cat "$LUN0_CONF"
} > "$WORK/gpt/partitions.conf"
# Fixed seed: the only random values left are the disk GUIDs and the unique
# GUIDs of Qualcomm's partitions, keep them reproducible
export PTOOL_SEED="eve-$BOARD"
export PYTHONPATH="$PTOOL_DIR"
python3 -m qcom_ptool.cli gen_partition -i "$WORK/gpt/partitions.conf" \
    -o "$WORK/gpt/partitions.xml"
if ! (cd "$WORK/gpt" && python3 -m qcom_ptool.cli ptool -x partitions.xml > ptool.log 2>&1); then
    tail -n 40 "$WORK/gpt/ptool.log" >&2
    exit 1
fi

# Sanity check the generated LUN 0 against the EVE layout
python3 /verify-gpt.py "$LUN0_CONF" "$WORK/gpt/gpt_main0.bin" "$WORK/gpt/gpt_backup0.bin"

for f in "$WORK"/gpt/gpt_main[0-9].bin "$WORK"/gpt/gpt_backup[0-9].bin \
         "$WORK"/gpt/gpt_both[0-9].bin "$WORK"/gpt/zeros_*.bin \
         "$WORK"/gpt/rawprogram[0-9].xml "$WORK"/gpt/patch[0-9].xml \
         "$WORK"/gpt/partitions.conf "$WORK"/gpt/partitions.xml; do
    install -m 0644 "$f" "$DEST/"
done

echo "$BOARD: $(find "$DEST" -type f | wc -l) files"
