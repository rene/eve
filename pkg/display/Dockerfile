# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM lfedge/eve-alpine:d075dd224fa7a7540817d224cc28d3cd998ad04c AS build

# The compositor stack. Deliberately minimal:
#   mesa-dri-gallium  - the DRI drivers; needed for GBM and EGL on the iGPU
#   weston            - the compositor. kiosk-shell.so (one client
#                       fullscreened per output) ships in this package on
#                       Alpine 3.22; there is no weston-shell-kiosk subpackage
#   seatd             - session/seat management; there is no logind here, so
#                       libseat falls back to its builtin backend, but seatd
#                       is kept available for a future multi-seat setup
#   libinput/libevdev - keyboard and pointer input
ENV PKGS="alpine-baselayout musl-utils \
    libdrm mesa mesa-dri-gallium mesa-egl mesa-gbm mesa-gles \
    wayland-libs-server wayland-libs-client wayland-libs-cursor wayland-libs-egl \
    libinput libinput-libs libevdev mtdev libxkbcommon pixman libffi libxml2 expat \
    seatd libseat \
    weston weston-backend-drm"
RUN eve-alpine-deploy.sh

COPY eve-compositor /out/opt/zededa/bin/eve-compositor
RUN chmod 755 /out/opt/zededa/bin/eve-compositor

FROM scratch
COPY --from=build /out/ /

# The compositor keeps its socket and generated config here; displaymgr
# writes the placement file into the same directory.
WORKDIR /run/display

ENTRYPOINT []
CMD ["/opt/zededa/bin/eve-compositor"]
