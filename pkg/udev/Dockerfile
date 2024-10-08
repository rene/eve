# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2024 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM lfedge/eve-alpine:82df60e43ab9f8c935584b8c7b4d0a4b0271d608 as build
ENV PKGS udev
RUN eve-alpine-deploy.sh

COPY etc/init.d/008-udevd /out/etc/init.d/
COPY etc/udev/rules.d/* /out/etc/udev/rules.d/

# Remove default rules from udev (we don't support many groups from these rules)
RUN rm /out/lib/udev/rules.d/*

FROM scratch
COPY --from=build /out/bin/udevadm /bin/
COPY --from=build /out/sbin/udevd /sbin/
COPY --from=build /out/lib/udev /lib/udev
COPY --from=build /out/etc/udev /etc/udev
COPY --from=build /out/etc/init.d/*  /etc/init.d/
COPY --from=build /out/lib/libblkid.so.1 /lib/
COPY --from=build /out/lib/libkmod.so.2 /lib/
COPY --from=build /out/usr/lib/libzstd.so.1 /usr/lib/
COPY --from=build /out/usr/lib/liblzma.so.5 /usr/lib/
COPY --from=build /out/lib/libz.so.1 /lib/
COPY --from=build /out/lib/libcrypto.so.1.1 /lib/
COPY --from=build /out/lib/libudev.so.1 /lib/

ENTRYPOINT []
CMD []
