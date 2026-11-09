# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

# Qualcomm userspace components required by QTI platforms (IQ8/IQ9). The
# versions follow the Qualcomm Linux BSP (QLI 2.1, meta-qcom) recipes, so EVE
# runs the same userspace as the reference BSP.
#
# NOTE: the QRTR name service and the protection-domain mapper are provided by
# the kernel (CONFIG_QRTR and CONFIG_QCOM_PD_MAPPER), as in the BSP, so the
# userspace qrtr-ns and pd-mapper are intentionally not shipped.

FROM lfedge/eve-alpine:edcb4dbc2f973b82676381a236165ce924149f2d AS build
ENV BUILD_PKGS="autoconf automake build-base bsd-compat-headers eudev-dev git glib-dev libbsd-dev libtool linux-headers meson pkgconf yaml-dev zstd-dev"
RUN eve-alpine-deploy.sh

# Staging directory: only what is installed here ends up in the final image
ENV STAGE=/qti-out

# libqrtr-glib (meta-openembedded: libqrtr-glib_git.bb, 1.2.2+git)
# Not used by the services below, kept for modem support (libqmi/ModemManager)
ENV LIBQRTR_GLIB_REV=8991f0e93713ebf4da48ae4f23940ead42f64c8c
ADD --keep-git-dir=true https://gitlab.freedesktop.org/mobile-broadband/libqrtr-glib.git#${LIBQRTR_GLIB_REV} /libqrtr-glib
WORKDIR /libqrtr-glib
RUN meson setup build --buildtype=release --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
          -Dgtk_doc=false -Dintrospection=false && \
    ninja -C build && \
    ninja -C build install && \
    DESTDIR="${STAGE}" ninja -C build install

# qrtr (meta-qcom: qrtr_1.2.bb)
ENV QRTR_VERSION=v1.2
ADD --keep-git-dir=true https://github.com/linux-msm/qrtr.git#${QRTR_VERSION} /qrtr
WORKDIR /qrtr
RUN meson setup build --buildtype=release --prefix=/usr --libdir=/usr/lib \
          -Dqrtr-ns=disabled -Dsystemd-service=disabled && \
    ninja -C build && \
    ninja -C build install && \
    DESTDIR="${STAGE}" ninja -C build install

# tqftpserv (meta-qcom: tqftpserv_1.2.bb)
ENV TQFTPSERV_VERSION=v1.2
ADD --keep-git-dir=true https://github.com/linux-msm/tqftpserv.git#${TQFTPSERV_VERSION} /tqftpserv
WORKDIR /tqftpserv
# systemd-unit-prefix must be set (no systemd here), the unit is discarded below
RUN meson setup build --buildtype=release --prefix=/usr \
          -Dsystemd-unit-prefix=/usr/lib/systemd/system && \
    ninja -C build && \
    DESTDIR="${STAGE}" ninja -C build install

# rmtfs (meta-qcom: rmtfs_1.3.bb)
ENV RMTFS_VERSION=v1.3
ADD --keep-git-dir=true https://github.com/linux-msm/rmtfs.git#${RMTFS_VERSION} /rmtfs
WORKDIR /rmtfs
# qmi_rmtfs.[ch] are already generated in-tree, touch them so make doesn't try
# to regenerate them with qmic (not available)
RUN touch qmi_rmtfs.c qmi_rmtfs.h && \
    make -j "$(getconf _NPROCESSORS_ONLN)" && \
    make prefix=/usr DESTDIR="${STAGE}" install

# fastrpc (meta-qcom: fastrpc_1.0.7.bb)
ENV FASTRPC_VERSION=v1.0.7
ADD --keep-git-dir=true https://github.com/qualcomm/fastrpc.git#${FASTRPC_VERSION} /fastrpc
WORKDIR /fastrpc
RUN autoreconf -fi && \
    ./configure --prefix=/usr && \
    make -j "$(getconf _NPROCESSORS_ONLN)" && \
    make DESTDIR="${STAGE}" install

# Drop everything not needed at runtime: headers, pkg-config/libtool files,
# man pages, systemd units/sysusers and udev rules (EVE has no systemd and the
# daemons are started by /etc/init.d/090-qti), and fastrpc test tools.
RUN rm -rf "${STAGE}/usr/include" \
           "${STAGE}/usr/lib/pkgconfig" \
           "${STAGE}/usr/share" \
           "${STAGE}/usr/lib/systemd" \
           "${STAGE}/usr/lib/udev" \
           "${STAGE}/usr/lib/sysusers.d" \
           "${STAGE}/lib" \
           "${STAGE}/usr/lib/fastrpc_test" \
           "${STAGE}/usr/bin/fastrpc_test" \
           "${STAGE}/usr/bin/dsp_check" && \
    find "${STAGE}" -name '*.la' -delete

# Pull in the runtime dependencies (glib, libbsd, yaml, zstd, eudev, ...) of
# everything staged above, except musl itself which is already in the rootfs.
RUN find "${STAGE}" -type f \( -perm -u+x -o -name '*.so*' \) -exec ldd {} \; 2>/dev/null | \
        awk '$2 == "=>" && $3 ~ /^\// { print $3 }' | sort -u > /runtime-libs && \
    while read -r lib; do \
        case "$lib" in \
            /lib/ld-musl-*|/lib/libc.musl-*) continue ;; \
        esac ; \
        [ -e "${STAGE}${lib}" ] && continue ; \
        mkdir -p "${STAGE}$(dirname "$lib")" && cp -L "$lib" "${STAGE}${lib}" ; \
    done < /runtime-libs && \
    find "${STAGE}" -type f \( -perm -u+x -o -name '*.so*' \) -exec sh -c 'strip "$1" 2>/dev/null || true' _ {} \; && \
    find "${STAGE}" \( -type f -o -type l \) | sort

FROM scratch
# hadolint ignore=DL3020
ADD rootfs/ /
COPY --from=build /qti-out/ /
