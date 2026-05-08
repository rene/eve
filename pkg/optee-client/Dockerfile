# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM lfedge/eve-alpine:b6b7e578e01a57d0067328d7b6a8db96d46edc23 AS build-arm64

ARG BUILD_PKGS="bash binutils-dev bsd-compat-headers build-base bc bison \
  cmake flex openssl-dev util-linux-dev linux-headers swig git gnutls-dev \
  argp-standalone jansson jansson-static mbedtls-dev mbedtls-static \
  perl python3 python3-dev py3-setuptools py3-pycryptodome py3-elftools pkgconfig"

ARG PKGS="argp-standalone jansson mbedtls-static"

# hadolint ignore=DL3006
RUN eve-alpine-deploy.sh

COPY patches /patches
COPY ftpm-tools /ftpm-tools

ENV OPTEE_CLIENT_VERSION=4.9.0
ADD --keep-git-dir=true https://github.com/OP-TEE/optee_client.git#${OPTEE_CLIENT_VERSION} /optee_client

WORKDIR /optee_client

# Apply patches for optee-client
RUN git config --global user.name 'Project EVE' && \
    git config --global user.email 'builder@projecteve.dev' && \
    for x in /patches/optee-client/*.patch; do \
      git am $x; \
    done

# Build tee-supplicant
RUN cmake -DCMAKE_INSTALL_PREFIX=/out/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make install

# Apply patches for ftpm-tools
WORKDIR /ftpm-tools
RUN for x in /patches/ftpm-tools/*.patch; do \
       patch -Np1 < $x; \
    done

# Build ftpm-helper
WORKDIR /ftpm-tools/ftpm-helper
RUN make -j "$(getconf _NPROCESSORS_ONLN)" && \
    mkdir -p /out/sbin && \
    cp host/output/ca/ftpm-helper/nvftpm-helper-app /out/sbin/nvftpm-helper-app

# Build sece_nv_ftpm_eval
WORKDIR /ftpm-tools/sece_nv_ftpm_eval
RUN make -j "$(getconf _NPROCESSORS_ONLN)" && \
    mkdir -p /out/sbin && \
    cp out/sece_nv_ftpm_eval /out/sbin/sece_nv_ftpm_eval

FROM scratch AS output-amd64
FROM scratch AS output-riscv64

FROM scratch AS output-arm64
ENTRYPOINT []
CMD []
ADD rootfs/ /
COPY --from=build-arm64 /out /

ARG TARGETARCH
FROM output-${TARGETARCH}
ENTRYPOINT []
CMD []
