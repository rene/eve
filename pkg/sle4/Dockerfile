# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM lfedge/eve-alpine:ee293fbdf7fe187d26721fed997cbbb5d7c11a46 AS build
ENV BUILD_PKGS="autoconf automake build-base bsd-compat-headers fts fts-dev glib glib-dev json-c json-c-dev libtool libnl3 libnl3-dev linux-headers libcap-dev git pkgconfig"
ENV PKGS="alpine-baselayout fts glib json-c libcap libnl3"
RUN eve-alpine-deploy.sh

# /data and /system files
ADD data_sys.tar.bz2 /

# Copy Android related headers and files
COPY prop-code/android_compat /root/android_compat

# Build qmi-framework
COPY prop-code/qmi-framework /qmi-framework
WORKDIR /qmi-framework
RUN ./autogen.sh && \
      CFLAGS="-DPTHREAD_MUTEX_RECURSIVE_NP=PTHREAD_MUTEX_RECURSIVE" ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# libcutils (we just need the headers)
ADD https://github.com/D-os/libcutils.git#master /libcutils
WORKDIR /libcutils
RUN cp -r include/cutils /usr/include/

# liblog (from Android)
COPY prop-code/liblog-eve /liblog
WORKDIR /liblog
RUN autoreconf -i && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# Build remotefs
COPY prop-code/remotefs-eve /remotefs
WORKDIR /remotefs
RUN autoreconf -i && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# Time services
COPY prop-code/time-services-eve /time-services
WORKDIR /time-services
RUN autoreconf -i && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install
WORKDIR /time-services/time-genoff
RUN ./autogen.sh && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# diag
COPY prop-code/diag-eve /diag
WORKDIR /diag
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc/ -I../include -I../src" LDFLAGS="-Wl,--no-as-needed" --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# xmllib
COPY prop-code/xmllib /xmllib
WORKDIR /xmllib
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc" --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# Linux headers
COPY prop-code/linux-headers /root/linux-headers

# data-oss
COPY prop-code/data-oss /data-oss
WORKDIR /data-oss
RUN autoreconf -i && \
      ./configure CFLAGS="-I/root/android_compat/common/inc" --with-sanitized-headers=/root/linux-headers --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# data
COPY prop-code/data-eve /data-eve
WORKDIR /data-eve/dsutils
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc" --with-sanitized-headers=/root/linux-headers --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

WORKDIR /data-eve/configdb
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc" --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# qmi
COPY prop-code/qmi-eve /qmi
WORKDIR /qmi
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc/ -DFEATURE_QMUXD_DISABLED -fforward-propagate" --with-qxdm --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

WORKDIR /qmi/qmi_client_helper
RUN ./autogen.sh && \
      ./configure CFLAGS="-I/root/android_compat/common/inc/" --with-qxdm --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# Build pd-mapper
COPY prop-code/pd-mapper-eve /pd-mapper
WORKDIR /pd-mapper
RUN autoreconf -i && \
      ./configure --without-systemd --enable-debug --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# ipacmdiag
WORKDIR /data-eve/ipacm-diag
RUN autoreconf -i && \
      ./configure CFLAGS="-I/root/android_compat/common/inc" --with-sanitized-headers=/root/linux-headers --with-glib --with-qxdm --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# ipacm-perf
#WORKDIR /data/ipacm
#RUN autoreconf -i && \
#      ./configure CFLAGS="-I/root/android_compat/common/inc" --with-sanitized-headers=/root/linux-headers --with-glib --with-qxdm --prefix=/usr && \
#      make -j "$(getconf _NPROCESSORS_ONLN)" && \
#      make -j "$(getconf _NPROCESSORS_ONLN)" install

# cld80211-lib
COPY prop-code/cld80211-lib-eve /cld80211-lib
WORKDIR /cld80211-lib
RUN autoreconf -i && \
      ./configure --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# cnss-daemon
COPY prop-code/cnss-daemon-eve /cnss-daemon
WORKDIR /cnss-daemon
RUN autoreconf -i && \
      ./configure --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

# tftp
COPY prop-code/tftp-eve /tftp
WORKDIR /tftp
RUN autoreconf -i && \
      ./configure --with-sanitized-headers=/root/linux-headers --with-glib --prefix=/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make -j "$(getconf _NPROCESSORS_ONLN)" install

FROM scratch
# hadolint ignore=DL3020
ADD rootfs/ /
COPY --from=build /usr/lib/* /usr/lib/
COPY --from=build /sbin/rmt_storage /sbin/
COPY --from=build /usr/bin/irsc_util /usr/bin/
COPY --from=build /usr/bin/qrtr-cfg /usr/bin/
COPY --from=build /usr/bin/qrtr-lookup /usr/bin/
COPY --from=build /usr/bin/qrtr-ns /usr/bin/
COPY --from=build /usr/bin/pdmappersvc /usr/bin/
COPY --from=build /usr/bin/time_daemon /usr/bin/
COPY --from=build /usr/bin/tftp_server /usr/bin/
COPY --from=build /usr/sbin/cnss_daemon /usr/sbin/
COPY --from=build /usr/bin/ipacmdiag /usr/bin/
COPY --from=build /data /data
COPY --from=build /system /system

