# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

FROM lfedge/eve-alpine:745ae9066273c73b0fd879c4ba4ff626a8392d04 as build

ARG BUILD_PKGS="bash binutils-dev bsd-compat-headers build-base bc bison \
  cmake flex openssl-dev util-linux-dev linux-headers swig git gnutls-dev \
  perl python3 python3-dev py3-setuptools py3-pycryptodome py3-elftools pkgconfig"

# hadolint ignore=DL3006
RUN eve-alpine-deploy.sh

COPY patches /patches

ENV OPTEE_CLIENT_VERSION=4.9.0
ADD --keep-git-dir=true https://github.com/OP-TEE/optee_client.git#${OPTEE_CLIENT_VERSION} /optee_client

WORKDIR /optee_client

# Apply patches
RUN git config --global user.name 'Project EVE' && \
    git config --global user.email 'builder@projecteve.dev' && \
    for x in /patches/*.patch; do \
      git am $x; \
    done

# Build tee-supplicant
RUN cmake -DCMAKE_INSTALL_PREFIX=/out/usr && \
      make -j "$(getconf _NPROCESSORS_ONLN)" && \
      make install

FROM scratch
ENTRYPOINT []
CMD []
COPY --from=build /out /
