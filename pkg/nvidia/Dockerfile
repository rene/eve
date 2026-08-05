# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2024 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

ARG PLATFORM=generic

FROM lfedge/eve-alpine:b79fd3384162534a9bdb99506adb2f19f60f26db AS build-base
ENV BUILD_PKGS="autoconf automake bash build-base coreutils gettext git glib-dev libtool libmd-dev m4 ncurses-dev tar xz-dev yq zstd-dev dpkg"

RUN eve-alpine-deploy.sh

# Set EVE_PLATFORM
ARG PLATFORM
ENV EVE_PLATFORM=$PLATFORM

# Setup git to apply patches
RUN git config --global user.email 'builder@projecteve.dev' && \
    git config --global user.name 'Project EVE'

# Jetpack tarballs
ENV JETSON_JP5=https://developer.nvidia.com/downloads/embedded/l4t/r35_release_v5.0/release/jetson_linux_r35.5.0_aarch64.tbz2
ENV JETSON_JP6=https://developer.nvidia.com/downloads/embedded/l4t/r36_release_v3.0/release/jetson_linux_r36.3.0_aarch64.tbz2
ENV JETSON_JP7=https://developer.nvidia.com/downloads/embedded/L4T/r38_Release_v4.0/release/Jetson_Linux_R38.4.0_aarch64.tbz2

# Default NVIDIA distro version
ENV NVIDIA_LINUX=${JETSON_JP5}
ENV NVIDIA_VER=jp5

# This is an arm64 (only) package, nothing to do for other architectures
FROM build-base AS amd64-generic
FROM build-base AS amd64-nvidia-jp5
FROM build-base AS amd64-nvidia-jp6
FROM build-base AS amd64-nvidia-jp7
FROM build-base AS amd64-nvidia-gb10
FROM build-base AS riscv64-generic
FROM build-base AS riscv64-nvidia-jp5
FROM build-base AS riscv64-nvidia-jp6
FROM build-base AS riscv64-nvidia-jp7
FROM build-base AS riscv64-nvidia-gb10

FROM build-base AS arm64-nvidia-jp5
ENV NVIDIA_LINUX=${JETSON_JP5}
ENV NVIDIA_VER=jp5
ADD ${NVIDIA_LINUX} /nvidia_linux.tbz2

FROM build-base AS arm64-nvidia-jp6
ENV NVIDIA_LINUX=${JETSON_JP6}
ENV NVIDIA_VER=jp6
ADD ${NVIDIA_LINUX} /nvidia_linux.tbz2

FROM build-base AS arm64-nvidia-jp7
ENV NVIDIA_LINUX=${JETSON_JP7}
ENV NVIDIA_VER=jp7
ADD ${NVIDIA_LINUX} /nvidia_linux.tbz2

FROM build-base AS arm64-nvidia-gb10
ENV NVIDIA_VER=gb10

ADD https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/sbsa/./libnvidia-egl-wayland1_1.1.21-1ubuntu1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-cfg1-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-compute-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-decode-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-encode-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-extra-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-fbc1-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/libnvidia-gl-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/nvidia-compute-utils-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/nvidia-firmware-580-580.159.03_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/nvidia-utils-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/
ADD http://ports.ubuntu.com/ubuntu-ports/pool/restricted/n/nvidia-graphics-drivers-580/xserver-xorg-video-nvidia-580_580.159.03-0ubuntu0.25.10.1_arm64.deb /Linux_for_Tegra/nv_tegra/l4t_deb_packages/

FROM ${TARGETARCH}-${PLATFORM} AS build

RUN echo "${EVE_PLATFORM}" > /eve-platform

# Extract tarball (when needed)
WORKDIR /
RUN test -f /nvidia_linux.tbz2 && tar -xjf /nvidia_linux.tbz2 || true

# Build nvidia-ctk
ENV NVIDIA_CONTAINER_TOOLKIT_REV=v1.19.0
ADD --keep-git-dir=true https://github.com/NVIDIA/nvidia-container-toolkit.git#${NVIDIA_CONTAINER_TOOLKIT_REV} /nvct

WORKDIR /nvct

COPY patches/* /nvct
RUN for x in /nvct/*.patch; do \
        git am "$x" ; \
    done

RUN mkdir -p dist
RUN make cmd-nvidia-ctk && cp nvidia-ctk dist/
RUN make cmd-nvidia-cdi-hook && cp nvidia-cdi-hook dist/
RUN make cmd-nvidia-container-runtime && cp nvidia-container-runtime dist/
RUN make cmd-nvidia-container-runtime-hook && cp nvidia-container-runtime-hook dist/

# Build nvfanctrl
COPY nvfanctrl/ /nvfanctrl
WORKDIR /nvfanctrl
RUN make && mkdir -p dist/ && cp nvfanctrl dist/

# Get and extract ldconfig for glibc, this tool runs over the rootfs of the
# application container, like CUDA application containers (glibc based).
ADD http://ports.ubuntu.com/ubuntu-ports/pool/main/g/glibc/libc-bin_2.31-0ubuntu9_arm64.deb /
RUN mkdir -p /ldconfig-bin && \
    dpkg -x /libc-bin_2.31-0ubuntu9_arm64.deb /ldconfig && \
    cp /ldconfig/sbin/ldconfig.real /ldconfig-bin/ldconfig-glibc

# Register source-built / downloaded NVIDIA components in the APK DB, mandatory
# for them to be included in the SBOM.
RUN case "${NVIDIA_VER}" in \
        jp5) JL_VER=r35.5.0 ;; \
        jp6) JL_VER=r36.3.0 ;; \
        jp7) JL_VER=r38.4.0 ;; \
        *)   JL_VER="${NVIDIA_VER}" ;; \
    esac && \
    if [ "${NVIDIA_VER:0:2}" == "jp" ]; then \
        register-sbom-pkg.sh -n jetson-linux -v "${JL_VER}" -l proprietary -u https://developer.nvidia.com/embedded/jetson-linux -o /sbom-out; \
    else \
        true; \
    fi && \
    register-sbom-pkg.sh -n nvidia-container-toolkit -v "${NVIDIA_CONTAINER_TOOLKIT_REV}" -l Apache-2.0 -u https://github.com/NVIDIA/nvidia-container-toolkit -o /sbom-out && \
    register-sbom-pkg.sh -n libc-bin -v 2.31-0ubuntu9 -l LGPL-2.1-or-later -u http://ports.ubuntu.com/ubuntu-ports/pool/main/g/glibc -o /sbom-out

# CDI Files
COPY cdi/${NVIDIA_VER} /cdi

# Copy udev rules
COPY udev/${NVIDIA_VER}/rules.d/* /rules.d/

# Copy scripts to processing CDI files and initialization
COPY scripts/*.sh /
COPY scripts/${NVIDIA_VER}/* /

# Process CDI files
WORKDIR /
RUN mkdir -p /ldconfig-bin /nvfanctrl/dist /nvct/dist /cdi /rootfs-dist /cdi-dist /rules.d && \
    mkdir -p /rootfs && \
    if [ -n "$(ls /cdi/* 2> /dev/null)" ]; then \
        for x in /cdi/*.yaml; do \
            OUTFILE=/cdi-dist/$(basename "$x") ; \
            /process-cdi.sh "$x" /rootfs /rootfs-dist &&  \
            yq '. | (.. | select(has("path")) | select(.path == "/usr/bin/nvidia-ctk")).path = "/opt/vendor/nvidia/bin/nvidia-ctk"' -i "$x" && \
            yq '. | (.. | select(has("path")) | select(.path == "/usr/bin/nvidia-cdi-hook")).path = "/opt/vendor/nvidia/bin/nvidia-cdi-hook"' -i "$x" && \
            yq '. | (.. | select(has("path")) | select(.path == "/usr/bin/nvidia-container-runtime")).path = "/opt/vendor/nvidia/bin/nvidia-container-runtime"' -i "$x" && \
            yq '. | (.. | select(has("path")) | select(.path == "/usr/bin/nvidia-container-runtime-hook")).path = "/opt/vendor/nvidia/bin/nvidia-container-runtime-hook"' -i "$x" && \
            /nvct/dist/nvidia-ctk cdi transform root --relative-to host \
                --from /usr --to /opt/vendor/nvidia/dist/usr --input "$x" | \
            /nvct/dist/nvidia-ctk cdi transform root --relative-to host \
                --from /etc --to /opt/vendor/nvidia/dist/etc | \
            /nvct/dist/nvidia-ctk cdi transform root --relative-to host \
                --from /lib --to /opt/vendor/nvidia/dist/lib --output "$OUTFILE" && \
            yq '. | (.. | select(has("args")) | select(.args[] == "update-ldcache")).args += ["--ldconfig-path", "/opt/vendor/nvidia/bin/ldconfig-glibc"]' -i "$OUTFILE" ; \
        done ; \
    fi && \
    rm -rf /rootfs/*

# Create libnvidia-ml.so.1 if doesn't exist
RUN if [ ! -f /rootfs-dist/usr/lib/aarch64-linux-gnu/libnvidia-ml.so.1 ]; then \
        cp /rootfs-dist/usr/lib/aarch64-linux-gnu/libnvidia-ml.so.580.159.03 \
           /rootfs-dist/usr/lib/aarch64-linux-gnu/libnvidia-ml.so.1 ; \
    fi

FROM scratch
COPY --from=build /cdi-dist/*.yaml /etc/cdi/
COPY --from=build /rules.d/* /opt/vendor/nvidia/etc/udev/rules.d/
COPY --from=build /rootfs-dist/ /opt/vendor/nvidia/dist/
COPY --from=build /ldconfig-bin/* /opt/vendor/nvidia/bin/
COPY --from=build /nvfanctrl/dist/* /opt/vendor/nvidia/bin/
COPY --from=build /nvct/dist/* /opt/vendor/nvidia/bin/
COPY --from=build /nv-dev*.sh /opt/vendor/nvidia/bin/
COPY --from=build /nv-init.sh /opt/vendor/nvidia/init.d/
COPY --from=build /eve-platform /opt/vendor/nvidia/
COPY --from=build /sbom-out/lib/apk/db/installed /lib/apk/db/installed

ENTRYPOINT []
CMD []
