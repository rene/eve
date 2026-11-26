# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# QDL flash bundle generator for Qualcomm IQ boards booting from UFS.
#
# At build time this package downloads the Qualcomm boot firmware (NHLOS: XBL,
# TZ, hypervisor, AOP, UEFI, ...) and the board CDT, at the same versions as
# the Qualcomm Linux BSP (QLI 2.1, meta-qcom), and qcom-ptool (with the
# patches in patches/, see README.md).
#
# At image build time, make-qcomflash takes a board directory (mounted at
# /board) and generates the GPT/QDL files with qcom-ptool (the board's or the
# base's layout for the firmware LUNs, EVE's layout for LUN 0 from
# partitions/eve-lun0.yaml). It adds the board's files and EVE's partition
# images (ESP, rootfs, config) and produces the directory to flash with qdl.

FROM lfedge/eve-alpine:ce3d0be803cf6912083f71ec01761123faf9144f AS build
# qcom-ptool needs PyYAML and jsonschema, at build time (base layouts) and when
# the bundle is made (GPT generation)
ENV BUILD_PKGS="patch python3 py3-jsonschema py3-yaml unzip"
ENV PKGS="coreutils dosfstools mtools python3 py3-jsonschema py3-yaml squashfs-tools"
RUN eve-alpine-deploy.sh

SHELL ["/bin/ash", "-eo", "pipefail", "-c"]

# qcom-ptool: upstream main, plus patches/ until they are merged upstream
ENV QCOM_PTOOL_REV=be4c6ad6fd69020052e0a743bc0230e91fa2f91c
ADD https://github.com/qualcomm-linux/qcom-ptool.git#${QCOM_PTOOL_REV} /qcom-ptool
COPY patches/ /patches/
WORKDIR /qcom-ptool
RUN for p in /patches/*.patch; do patch -p1 --no-backup-if-mismatch < "$p"; done

# Boot firmware (meta-qcom: firmware-qcom-boot-qcs8300_00142.bb,
# firmware-qcom-boot-qcs9100_00142.bb) and CDT (firmware-qcom-cdt-qcs8300.bb,
# firmware-qcom-cdt-qcs9100.bb)
ENV BOOTBIN_VER=00142
ENV FW_URL=https://softwarecenter.qualcomm.com/nexus/generic/product/chip/tech-package
ENV CDT_URL=https://artifacts.codelinaro.org/artifactory/codelinaro-le/Qualcomm_Linux
ADD ${FW_URL}/QCS8300_bootbinaries.1.0/qcs8300_bootbinaries.1.0-test-device-public/${BOOTBIN_VER}/QCS8300_bootbinaries_${BOOTBIN_VER}.zip /dl/
ADD ${FW_URL}/QCS8300_bootbinaries.1.0/qcs8300_bootbinaries.1.0-test-device-public/${BOOTBIN_VER}/QCS8300_bootbinaries_immutable_${BOOTBIN_VER}.zip /dl/
ADD ${FW_URL}/QCS9100_bootbinaries.1.0/qcs9100_bootbinaries.1.0-test-device-public/${BOOTBIN_VER}/QCS9100_bootbinaries_${BOOTBIN_VER}.zip /dl/
ADD ${FW_URL}/QCS9100_bootbinaries.1.0/qcs9100_bootbinaries.1.0-test-device-public/${BOOTBIN_VER}/QCS9100_bootbinaries_immutable_${BOOTBIN_VER}.zip /dl/
ADD ${CDT_URL}/QCS8300/cdt/qcs8275-iq-8275-evk-pro-sku.zip /dl/cdt-iq-8275-evk.zip
ADD ${CDT_URL}/QCS9100/cdt/rb8_core_kit.zip /dl/cdt-iq-9075-evk.zip
COPY downloads.sha256 /dl/
WORKDIR /dl
RUN sha256sum -c downloads.sha256

# Static part of the flash bundles, one directory per Qualcomm reference
# board ("base"): boot firmware, CDT and the default partition layout
COPY prepare-base.sh /
COPY qcom-layout /out/usr/bin/
ENV DL_DIR=/dl PTOOL_DIR=/qcom-ptool OUT_DIR=/out/qcom-flash QCOM_LAYOUT=/out/usr/bin/qcom-layout
WORKDIR /
RUN /prepare-base.sh iq-8275-evk qcs8300 cdt_qcs8275_iq_8275_evk_pro_sku.bin && \
    /prepare-base.sh iq-9075-evk qcs9100 cdt_rb8_core_kit.bin

# The GPT is generated when the bundle is made, from the board's partition
# layout or the base's one, so ship the patched qcom-ptool. Boards are not part
# of this image (they may hold proprietary files), they are mounted at run time.
RUN mkdir -p /out/qcom-flash/ptool && \
    cp -r /qcom-ptool/qcom_ptool /qcom-ptool/LICENSE* /out/qcom-flash/ptool/
COPY partitions/eve-lun0.yaml verify-gpt.py grub-qcom.cfg /out/qcom-flash/
COPY qcom-gen-gpt qcom-bundle-check /out/usr/bin/

# Sanity check: generate the partition tables of every base
ENV PTOOL_DIR=/out/qcom-flash/ptool LUN0_YAML=/out/qcom-flash/eve-lun0.yaml VERIFY_GPT=/out/qcom-flash/verify-gpt.py
RUN for l in /out/qcom-flash/layouts/*.yaml; do \
        mkdir -p "/tmp/gpt/$(basename "$l")" && \
        PATH="/out/usr/bin:$PATH" qcom-gen-gpt "$l" "/tmp/gpt/$(basename "$l")" test; \
    done && rm -rf /tmp/gpt

COPY make-qcomflash trim-image /out/usr/bin/
RUN echo "mtools_skip_check=1" >> /out/etc/mtools.conf && \
    mkdir -p /out/parts /out/out

# Register the downloaded components in the APK DB, mandatory for them to be
# included in the SBOM.
RUN register-sbom-pkg.sh -n qcs8300-bootbinaries -v "${BOOTBIN_VER}" -l proprietary -u "${FW_URL}/QCS8300_bootbinaries.1.0" -o /out && \
    register-sbom-pkg.sh -n qcs9100-bootbinaries -v "${BOOTBIN_VER}" -l proprietary -u "${FW_URL}/QCS9100_bootbinaries.1.0" -o /out && \
    register-sbom-pkg.sh -n qcom-cdt -v "qli-2.1" -l BSD-3-Clause -u "${CDT_URL}" -o /out && \
    register-sbom-pkg.sh -n qcom-ptool -v "${QCOM_PTOOL_REV}" -l BSD-3-Clause-Clear -u https://github.com/qualcomm-linux/qcom-ptool -o /out

FROM scratch
COPY --from=build /out/ /
WORKDIR /
ENTRYPOINT ["/usr/bin/make-qcomflash"]
