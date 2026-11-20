# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# QDL flash bundle generator for Qualcomm IQ boards booting from UFS.
#
# At build time this package downloads the Qualcomm boot firmware (NHLOS: XBL,
# TZ, hypervisor, AOP, UEFI, ...) and the board CDT, at the same versions as
# the Qualcomm Linux BSP (QLI 2.1, meta-qcom), and the patched qcom-ptool.
#
# At image build time, make-qcomflash takes a board directory (mounted at
# /board) and generates the GPT/QDL files with qcom-ptool (the board's or the
# base's layout for the firmware LUNs, EVE's layout for LUN 0 from
# partitions/eve-lun0.conf). It adds the board's files and EVE's partition
# images (ESP, rootfs, config) and produces the directory to flash with qdl.

FROM lfedge/eve-alpine:61283a3a978a7cdc9073a1ba678717fc3ea1fe97 AS build
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

# Static part of the flash bundles, one directory per Qualcomm reference
# board ("base"): boot firmware, CDT and the default partition layout
COPY prepare-base.sh /
ENV DL_DIR=/dl PTOOL_DIR=/qcom-ptool OUT_DIR=/out/qcom-flash
WORKDIR /
RUN /prepare-base.sh iq-8275-evk qcs8300 cdt_qcs8275_iq_8275_evk_pro_sku.bin && \
    /prepare-base.sh iq-9075-evk qcs9100 cdt_rb8_core_kit.bin

# The GPT is generated when the bundle is made, from the board's partition
# layout or the base's one, so ship the patched qcom-ptool. Boards are not part
# of this image (they may hold proprietary files), they are mounted at run time.
RUN mkdir -p /out/qcom-flash/ptool && \
    cp -r /qcom-ptool/qcom_ptool /qcom-ptool/LICENSE* /out/qcom-flash/ptool/
COPY partitions/eve-lun0.conf verify-gpt.py grub-qcom.cfg /out/qcom-flash/
COPY qcom-gen-gpt qcom-bundle-check /out/usr/bin/

# Sanity check: generate the partition tables of every base
ENV PTOOL_DIR=/out/qcom-flash/ptool LUN0_CONF=/out/qcom-flash/eve-lun0.conf VERIFY_GPT=/out/qcom-flash/verify-gpt.py
RUN for l in /out/qcom-flash/layouts/*.conf; do \
        mkdir -p "/tmp/gpt/$(basename "$l")" && \
        /out/usr/bin/qcom-gen-gpt "$l" "/tmp/gpt/$(basename "$l")" test; \
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
