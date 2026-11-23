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

# fastrpc (meta-qcom: fastrpc_1.0.7.bb), installed with the other Hexagon DSP
# components in /opt/vendor/qcom: daemons in bin/, libraries in lib/ (found
# through the rpath) and the DSP libraries/configuration in dsp/
ENV FASTRPC_VERSION=v1.0.7
ENV QCOM_VENDOR_DIR=/opt/vendor/qcom
ADD --keep-git-dir=true https://github.com/qualcomm/fastrpc.git#${FASTRPC_VERSION} /fastrpc
WORKDIR /fastrpc
RUN autoreconf -fi && \
    ./configure --prefix="${QCOM_VENDOR_DIR}" --sbindir="${QCOM_VENDOR_DIR}/bin" \
                --with-config-base-dir="${QCOM_VENDOR_DIR}/dsp/" \
                LDFLAGS="-Wl,-rpath,${QCOM_VENDOR_DIR}/lib" && \
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
           "${STAGE}${QCOM_VENDOR_DIR}/include" \
           "${STAGE}${QCOM_VENDOR_DIR}/lib/pkgconfig" \
           "${STAGE}${QCOM_VENDOR_DIR}/share" \
           "${STAGE}${QCOM_VENDOR_DIR}/lib/fastrpc_test" \
           "${STAGE}${QCOM_VENDOR_DIR}/bin/fastrpc_test" \
           "${STAGE}${QCOM_VENDOR_DIR}/bin/dsp_check" && \
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

# Hexagon DSP libraries and FastRPC shells (meta-qcom:
# hexagon-dsp-binaries_20260910.bb), installed to /opt/vendor/qcom/dsp, the
# configuration base directory of fastrpc (conf.d/*.yaml map the device tree
# model to the board's DSP_LIBRARY_PATH). They must match the DSP firmware,
# linux-firmware 20260910 in pkg/fw. Only the IQ-8275 EVK and IQ-9075 EVK,
# plus the reference boards their directories link to, are installed. The
# meta-qcom patch makes the IQ-8275 EVK CDSP use the QCS8300 libraries
# (submitted upstream as linux-msm/hexagon-dsp-binaries#117), without it
# FastRPC can't start a CDSP user PD on that board.
# Separate stage: these are Hexagon (DSP) binaries, nothing to build, and they
# must not go through the ldd/strip step of the build stage.
FROM lfedge/eve-alpine:edcb4dbc2f973b82676381a236165ce924149f2d AS dsp-binaries
ENV BUILD_PKGS="coreutils patch"
RUN eve-alpine-deploy.sh
SHELL ["/bin/ash", "-eo", "pipefail", "-c"]
ENV DSP_BINARIES_VERSION=20260910
ENV DSP_BINARIES_REV=bbb0e6b59143e052dbf26b371ee239ab4b0881b0
ENV DSP_DIR=/dsp-out/opt/vendor/qcom/dsp
ADD https://github.com/linux-msm/dsp-binaries.git#${DSP_BINARIES_REV} /dsp-binaries
COPY patches/dsp-binaries/ /patches/dsp-binaries/
WORKDIR /dsp-binaries
RUN for p in /patches/dsp-binaries/*.patch; do patch -p1 < "$p"; done && \
    awk -v re='^(sa8775p/Qualcomm/(SA8775P-RIDE|IQ9075-EVK)|qcs8300/Qualcomm/(QCS8300-RIDE|IQ8275-EVK))(/|$)' \
        '($1 == "Install:" && $2 ~ re) || ($1 == "Link:" && $3 ~ re)' config.txt > /dsp-config.txt && \
    cat /dsp-config.txt && \
    ./scripts/install.sh /dsp-config.txt "${DSP_DIR}" && \
    install -d "${DSP_DIR}/conf.d" && \
    for b in iq8275-evk iq9075-evk qcs8300-ride sa8775p-ride; do \
        install -m 0644 "conf.d/hexagon-dsp-binaries-qualcomm-$b.yaml" "${DSP_DIR}/conf.d/" ; \
    done && \
    install -m 0644 conf.d/schema.json "${DSP_DIR}/conf.d/" && \
    install -m 0644 LICENSE.qcom LICENSE.qcom-2 WHENCE "${DSP_DIR}/"
# No dangling symlinks (a Link: whose target wasn't installed)
RUN dangling=$(find -L "${DSP_DIR}" -type l) && \
    { [ -z "$dangling" ] || { echo "dangling symlinks: $dangling" >&2; exit 1; }; } && \
    find "${DSP_DIR}" -maxdepth 5 \( -type d -o -type l \) | sort && \
    du -shL "${DSP_DIR}"

# FastRPC client libraries for edge app containers, which run on glibc
# (Debian, Ubuntu, ...) and can't load the musl ones above: the same fastrpc
# version, from Debian (trixie-backports), with the libraries it needs. The
# qualcomm.com/dsp CDI devices (qcom-dsp-cdi) mount them in
# /usr/lib/aarch64-linux-gnu, so they need glibc >= 2.38 in the container.
# Pinned on snapshot.debian.org, which keeps every package version. Only arm64
# devices use them, other architectures get nothing.
FROM lfedge/eve-alpine:edcb4dbc2f973b82676381a236165ce924149f2d AS fastrpc-glibc-arm64
ENV BUILD_PKGS="dpkg xz"
RUN eve-alpine-deploy.sh
SHELL ["/bin/ash", "-eo", "pipefail", "-c"]
ENV DEBIAN_SNAPSHOT=https://snapshot.debian.org/archive/debian/20261120T000000Z
ADD ${DEBIAN_SNAPSHOT}/pool/contrib/f/fastrpc/libfastrpc1_1.0.7-2~bpo13+1_arm64.deb \
    ${DEBIAN_SNAPSHOT}/pool/main/liby/libyaml/libyaml-0-2_0.2.5-2_arm64.deb \
    ${DEBIAN_SNAPSHOT}/pool/main/libb/libbsd/libbsd0_0.12.2-2_arm64.deb \
    ${DEBIAN_SNAPSHOT}/pool/main/libm/libmd/libmd0_1.1.0-2+b1_arm64.deb \
    /debs/
WORKDIR /debs
ENV OUT=/glibc-out/opt/vendor/qcom/glibc
# Each library is installed under its soname, with its Debian copyright file.
# The default listeners are left out: only the FastRPC daemons use them.
RUN printf '%s  %s\n' \
        35df085e1abb15abfba103c9947d86a686fb530c3d383a7f5e10834d51a1df26 'libfastrpc1_1.0.7-2~bpo13+1_arm64.deb' \
        c81500b4c93e29ac28cdded7ec5bf9950c75f8c76c386669ccb6fb854c4daf72 libyaml-0-2_0.2.5-2_arm64.deb \
        6827c1b3ad8dcbf3478803f9150359433e3376a3bde01c4503888a22eb3924e3 libbsd0_0.12.2-2_arm64.deb \
        772e86842fd3fa1816a0cfee4833a73996aaa550461491543b886b9c8f999052 'libmd0_1.1.0-2+b1_arm64.deb' | \
        sha256sum -c
# A plain loop (not in an && list), so that any failing command stops the build
RUN mkdir -p "${OUT}/lib" /sbom-out /x ; \
    for deb in *.deb; do \
        pkg=$(dpkg-deb -f "$deb" Package) ; \
        dpkg-deb -x "$deb" "/x/$pkg" ; \
        install -m 0644 "/x/$pkg/usr/share/doc/$pkg/copyright" "${OUT}/LICENSE.$pkg" ; \
        find "/x/$pkg/usr/lib/aarch64-linux-gnu" -type l -name 'lib*.so.[0-9]*' ! -name '*_default_listener*' | \
            while read -r l; do install -m 0644 "$(readlink -f "$l")" "${OUT}/lib/${l##*/}"; done ; \
        case "$pkg" in \
            libfastrpc1) lic="BSD-3-Clause AND BSD-1-Clause" ;; \
            libyaml-0-2) lic="MIT" ;; \
            libbsd0) lic="BSD-3-Clause AND BSD-2-Clause AND ISC AND MIT AND Beerware AND LicenseRef-Public-Domain" ;; \
            libmd0) lic="BSD-3-Clause AND BSD-2-Clause AND ISC AND Beerware AND LicenseRef-Public-Domain" ;; \
            *) echo "no license for $pkg" >&2 ; exit 1 ;; \
        esac ; \
        src=$(dpkg-deb -f "$deb" Source | cut -d ' ' -f 1) ; \
        register-sbom-pkg.sh -n "$pkg" -v "$(dpkg-deb -f "$deb" Version)" -l "$lic" \
            -u "https://tracker.debian.org/pkg/${src:-$pkg}" -o /sbom-out ; \
    done ; \
    rm -rf /x ; \
    for l in libadsprpc.so.1 libcdsprpc.so.1 libsdsprpc.so.1 libyaml-0.so.2 libbsd.so.0 libmd.so.0; do \
        [ -f "${OUT}/lib/$l" ] || { echo "$l missing" >&2 ; exit 1 ; } ; \
    done ; \
    ls -l "${OUT}" "${OUT}/lib"

FROM lfedge/eve-alpine:edcb4dbc2f973b82676381a236165ce924149f2d AS fastrpc-glibc-amd64
RUN mkdir -p /glibc-out /sbom-out/lib/apk/db && touch /sbom-out/lib/apk/db/installed

FROM fastrpc-glibc-amd64 AS fastrpc-glibc-riscv64

# hadolint ignore=DL3006
FROM fastrpc-glibc-${TARGETARCH} AS fastrpc-glibc

FROM scratch
# hadolint ignore=DL3020
ADD rootfs/ /
COPY --from=build /qti-out/ /
COPY --from=dsp-binaries /dsp-out/ /
COPY --from=fastrpc-glibc /glibc-out/ /
COPY --from=fastrpc-glibc /sbom-out/lib/apk/db/installed /lib/apk/db/installed
