# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2025 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

# Yetus image with all dependencies needed for EVE project

FROM ghcr.io/apache/yetus:main as build

ENV PKG_DEPS="alien autoconf automake build-essential debhelper dh-autoreconf dh-python dkms fakeroot gawk git libaio-dev libattr1-dev libblkid-dev libcurl4-openssl-dev libelf-dev libffi-dev libpam0g-dev libssl-dev libtirpc-dev libtool libudev-dev linux-headers-generic parallel po-debconf python3 python3-all-dev python3-cffi python3-dev python3-packaging python3-setuptools python3-sphinx uuid-dev zlib1g-dev"

# should be aligned with kernel
#  * ZFS on Linux
ENV ZFS_VERSION=2.3.3
ENV ZFS_COMMIT=zfs-${ZFS_VERSION}
ENV ZFS_REPO=https://github.com/openzfs/zfs.git

WORKDIR /tmp/zfs
# hadolint ignore=DL3020
ADD ${ZFS_REPO}#${ZFS_COMMIT} /tmp/zfs

# hadolint ignore=DL3008
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
 && apt-get install --no-install-recommends -y $PKG_DEPS \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN ./autogen.sh && \
    ./configure \
    --prefix=/usr \
    --with-tirpc \
    --sysconfdir=/etc \
    --mandir=/usr/share/man \
    --infodir=/usr/share/info \
    --localstatedir=/var \
    --with-config=user \
    --with-udevdir=/lib/udev \
    --disable-systemd \
    --disable-static && \
    ./scripts/make_gitrev.sh && \
    make -j "$(getconf _NPROCESSORS_ONLN)" && \
    make DESTDIR=/ install-strip

# Clean up
RUN rm -r /tmp/zfs

ENTRYPOINT ["/entrypoint.sh"]
