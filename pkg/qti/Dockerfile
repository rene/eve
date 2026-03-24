# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM lfedge/eve-alpine:5012b22e49d947e0e45e1f5b3a65ebbeb89dec96 AS build
ENV BUILD_PKGS="autoconf automake build-base bsd-compat-headers eudev-dev eudev-libs glib glib-dev json-c json-c-dev libbsd libbsd-dev libtool linux-headers git meson pkgconfig xz-dev xz-libs yaml yaml-dev"
ENV PKGS="alpine-baselayout eudev-libs fts libbsd glib json-c xz-libs yaml"
RUN eve-alpine-deploy.sh

# Build qmi-framework
ENV QMI_FRAMEWORK_VERSION=v0.1.3
ADD --keep-git-dir=true https://github.com/quic/qmi-framework.git#${QMI_FRAMEWORK_VERSION} /qmi-framework
WORKDIR /qmi-framework
RUN autoreconf --install && \
      CFLAGS="-DPTHREAD_MUTEX_RECURSIVE_NP=PTHREAD_MUTEX_RECURSIVE -D_BSD_SOURCE -include endian.h" ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# Build libqrtr
ENV LIBQRTR_REV=8991f0e93713ebf4da48ae4f23940ead42f64c8c
ADD --keep-git-dir=true https://gitlab.freedesktop.org/mobile-broadband/libqrtr-glib.git#${LIBQRTR_REV} /libqrtr
WORKDIR /libqrtr
RUN meson build --buildtype=release --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
          -Dgtk_doc=false -Dintrospection=false && \
    ninja -C build && \
    ninja -C build install

# Build qrtr
ENV QRTR_REV=d0d471c96e7d112fac6f48bd11f9e8ce209c04d2
ADD --keep-git-dir=true https://github.com/linux-msm/qrtr.git#${QRTR_REV} /qrtr
WORKDIR /qrtr
RUN make prefix=/usr && make prefix=/usr install

# Build pd-mapper
ENV PD_MAPPER_REV=10997ba7c43a3787a40b6b1b161408033e716374
ADD --keep-git-dir=true https://github.com/linux-msm/pd-mapper.git#${PD_MAPPER_REV} /pd_mapper
WORKDIR /pd_mapper
RUN make prefix=/usr && make prefix=/usr install

# tqftpserv
ENV TQFTP_REV=de42697a2466cc5ee267ffe36ab4e8494f005fb0
ADD --keep-git-dir=true https://github.com/linux-msm/tqftpserv.git#${TQFTP_REV} /tftp
WORKDIR /tftp
RUN make prefix=/usr && make prefix=/usr install

# fastrpc
ENV FASTRPC_REV=8572ae1c45d38a4dc8853b1b9b6738207ab1ce94
ADD --keep-git-dir=true https://github.com/qualcomm/fastrpc.git#${FASTRPC_REV} /fastrpc
WORKDIR /fastrpc
RUN autoreconf -i && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

FROM scratch
# hadolint ignore=DL3020
ADD rootfs/ /
COPY --from=build /usr/lib/* /usr/lib/
COPY --from=build /usr/bin/qrtr-cfg /usr/bin/
COPY --from=build /usr/bin/qrtr-lookup /usr/bin/
COPY --from=build /usr/bin/qrtr-ns /usr/bin/
COPY --from=build /usr/bin/pd-mapper /usr/bin/
COPY --from=build /usr/bin/tqftpserv /usr/bin/
COPY --from=build /usr/bin/adsprpcd /usr/bin/
COPY --from=build /usr/bin/cdsprpcd /usr/bin/
COPY --from=build /usr/bin/gdsprpcd /usr/bin/
COPY --from=build /usr/bin/sdsprpcd /usr/bin/

