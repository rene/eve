# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# QDL flash bundle generator for Qualcomm IQ boards booting from UFS.
#
# At build time this package downloads the Qualcomm boot firmware (NHLOS: XBL,
# TZ, hypervisor, AOP, UEFI, ...) and the board CDT, at the same versions as
# the Qualcomm Linux BSP (QLI 2.1, meta-qcom), and generates the GPT/QDL files
# with qcom-ptool: Qualcomm's layout for the firmware LUNs and EVE's layout
# for LUN 0 (partitions/eve-lun0.conf).
#
# At image build time, make-qcomflash adds EVE's partition images (ESP,
# rootfs, config) and produces the directory to be flashed with qdl.

FROM lfedge/eve-alpine:4635a9f93267ec0e24005aa80f2e7b1b828e5d17 AS build
ENV BUILD_PKGS="patch python3 unzip"
ENV PKGS="coreutils dosfstools mtools python3"
RUN eve-alpine-deploy.sh

SHELL ["/bin/ash", "-eo", "pipefail", "-c"]

# qcom-ptool (meta-qcom: recipes-bsp/partition/qcom-ptool.inc)
ENV QCOM_PTOOL_REV=2dec95bc7bf76efb5dddb392cc3bc241fd272f65
ADD https://github.com/qualcomm-linux/qcom-ptool.git#${QCOM_PTOOL_REV} /qcom-ptool
COPY patches/ /patches/
WORKDIR /qcom-ptool
RUN for p in /patches/*.patch; do patch -p1 < "$p"; done

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

# Static part of the flash bundles, one directory per board
COPY partitions/eve-lun0.conf /eve-lun0.conf
COPY prepare-board.sh verify-gpt.py /
ENV DL_DIR=/dl PTOOL_DIR=/qcom-ptool LUN0_CONF=/eve-lun0.conf OUT_DIR=/out/qcom-flash
WORKDIR /
RUN /prepare-board.sh iq-8275-evk qcs8300 cdt_qcs8275_iq_8275_evk_pro_sku.bin && \
    /prepare-board.sh iq-9075-evk qcs9100 cdt_rb8_core_kit.bin

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
