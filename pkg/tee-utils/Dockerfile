# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

# use the same set of packages for simplicity
ARG BUILD_PKGS_BASE="autoconf automake bash binutils binutils-dev build-base bc bison bsd-compat-headers \
                     curl dtc expat flex gnutls-dev linux-headers openssl-dev \
                     perl python3 py3-pycryptodome swig util-linux-dev"

# we use the same image in several places
ARG EVE_ALPINE_IMAGE=lfedge/eve-alpine:30eeeb121f7ab2fffbdcda3ce7e2a625d88a9677

# OPTEE-OS images
FROM lfedge/eve-optee-os:150dfb58cd0fc2b781aa8e700d479e369c8cc5e9 AS optee-os

# hadolint ignore=DL3006
FROM ${EVE_ALPINE_IMAGE} as build-native
ARG BUILD_PKGS_BASE
RUN BUILD_PKGS="${BUILD_PKGS_BASE}" eve-alpine-deploy.sh

# hadolint ignore=DL3006,DL3029
FROM --platform=${BUILDPLATFORM} ${EVE_ALPINE_IMAGE} as build-cross
ARG BUILD_PKGS_BASE
RUN BUILD_PKGS="${BUILD_PKGS_BASE}" eve-alpine-deploy.sh

# hadolint ignore=DL3029
FROM --platform=${BUILDPLATFORM} lfedge/eve-cross-compilers:250abc77c8c39664905b66a2673102ec5cd3b056 AS cross-compilers

# will use several packages from target arch and copy them to sysroot
# hadolint ignore=DL3006
FROM ${EVE_ALPINE_IMAGE} AS cross-compile-libs
ENV PKGS musl-dev libgcc libintl libuuid libtirpc libblkid libattr linux-headers bsd-compat-headers
RUN eve-alpine-deploy.sh

# adjust EVE_TARGET_ARCH for cross-compiler
FROM build-cross AS build-cross-target-amd64
ENV EVE_TARGET_ARCH=x86_64
FROM build-cross AS build-cross-target-arm64
ENV EVE_TARGET_ARCH=aarch64

# hadolint ignore=DL3006
FROM build-cross-target-${TARGETARCH} AS build-cross-target
ENV CROSS_COMPILE_ENV="${EVE_TARGET_ARCH}"-alpine-linux-musl-
ENV CROSS_COMPILE_ARM32_ENV="armv6-alpine-linux-musleabihf-"
COPY --from=cross-compilers /packages /packages
# hadolint ignore=DL3018
RUN apk add --no-cache --allow-untrusted -X /packages build-base-"${EVE_TARGET_ARCH}" build-base-armhf
COPY --from=cross-compile-libs /out/ /usr/"${EVE_TARGET_ARCH}"-alpine-linux-musl/

# cross-compilers
FROM build-cross-target AS target-arm64-build-amd64
FROM build-cross-target AS target-amd64-build-arm64
# native
FROM build-native AS target-amd64-build-amd64
FROM build-native AS target-arm64-build-arm64

# hadolint ignore=DL3006
FROM target-${TARGETARCH}-build-${BUILDARCH} AS build

SHELL ["/bin/bash", "-eo", "pipefail", "-c"]

RUN mkdir /tee-utils /tee-tarballs /tee-dist

# Copy optee-os artifacts
COPY --from=optee-os /opteeos_images /opteeos_images

WORKDIR /tee-utils

# OPTEE-OS targets
ENV OPTEEOS_TARGETS="imx8mp_pollux imx8mp_epc_r3720"

# i.MX8MP
ENV OPTEECLIENT_REV_imx8mp_pollux=182874320395787a389e5b0f7df02b32f3c0a1b0
ENV OPTEETESTS_REV_imx8mp_pollux=4d81b964a72e89a62d04187b3f055d8346b383c9
ENV OPTEEDEMOS_REV_imx8mp_pollux=a98d01e1b9168eaed96bcd0bac0df67c44a81081
ENV TEE_SRC_imx8mp_pollux="pollux"

ENV OPTEECLIENT_REV_imx8mp_epc_r3720=182874320395787a389e5b0f7df02b32f3c0a1b0
ENV OPTEETESTS_REV_imx8mp_epc_r3720=4d81b964a72e89a62d04187b3f055d8346b383c9
ENV OPTEEDEMOS_REV_imx8mp_epc_r3720=a98d01e1b9168eaed96bcd0bac0df67c44a81081
ENV TEE_SRC_imx8mp_epc_r3720="epc_r3720"

# Repositories
ADD https://github.com/nxp-imx/imx-optee-client.git#${OPTEECLIENT_REV_imx8mp_pollux} ${TEE_SRC_imx8mp_pollux}/optee_client
ADD https://github.com/nxp-imx/imx-optee-test.git#${OPTEETESTS_REV_imx8mp_pollux} ${TEE_SRC_imx8mp_pollux}/optee_test
ADD https://github.com/linaro-swg/optee_examples.git#${OPTEEDEMOS_REV_imx8mp_pollux} ${TEE_SRC_imx8mp_pollux}/optee_examples
ADD https://github.com/nxp-imx/imx-optee-client.git#${OPTEECLIENT_REV_imx8mp_epc_r3720} ${TEE_SRC_imx8mp_epc_r3720}/optee_client
ADD https://github.com/nxp-imx/imx-optee-test.git#${OPTEETESTS_REV_imx8mp_epc_r3720} ${TEE_SRC_imx8mp_epc_r3720}/optee_test
ADD https://github.com/linaro-swg/optee_examples.git#${OPTEEDEMOS_REV_imx8mp_epc_r3720} ${TEE_SRC_imx8mp_epc_r3720}/optee_examples

# hadolint ignore=DL3003,SC2086
RUN for t in ${OPTEEOS_TARGETS}; do \
        [ "$EVE_TARGET_ARCH" != "aarch64" ] && break ;\
        repo=$(eval echo \$TEE_SRC_${t}) ;\
        TA_DEV_KIT_DIR="/opteeos_images/${t}/export-ta_arm64/" ;\
        OPTEE_CLIENT_EXPORT="/tee-utils/${repo}/optee_client/out/export/usr" ;\
        (cd ${repo}/optee_client ;\
         make clean && \
         make -j "$(getconf _NPROCESSORS_ONLN)" \
            ARCH=arm64 \
            CROSS_COMPILE="${CROSS_COMPILE_ENV}" \
            O=out && \
         (cd out/export ; tar -cvf "/tee-tarballs/${t}_client.tar" usr)) ;\
         (cd ${repo}/optee_test ;\
         make -j "$(getconf _NPROCESSORS_ONLN)" \
            ARCH=arm64 \
            CROSS_COMPILE_HOST="${CROSS_COMPILE_ENV}" \
            CROSS_COMPILE_TA="${CROSS_COMPILE_ENV}" \
            CROSS_COMPILE="${CROSS_COMPILE_ENV}" \
            TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR}" \
            TA_SIGN_KEY="/opteeos_images/eve-optee.pem" \
            OPTEE_CLIENT_EXPORT="${OPTEE_CLIENT_EXPORT}" ;\
         mkdir -p dist/lib/optee_armtz dist/usr/lib/tee-supplicant/plugins dist/usr/bin ;\
         cp -v out/xtest/xtest dist/usr/bin/ ;\
         cp -v out/supp_plugin/*.plugin dist/usr/lib/tee-supplicant/plugins/ ;\
         find out/ta -name "*.ta" -exec cp -v {} dist/lib/optee_armtz/ \; ;\
         find "${TA_DEV_KIT_DIR}" -name "*.ta" -exec cp -v {} dist/lib/optee_armtz \; ;\
         (cd dist/ ; tar -cvf "/tee-tarballs/${t}_xtest.tar" ./)) ;\
         (cd ${repo}/optee_examples ;\
         make -j "$(getconf _NPROCESSORS_ONLN)" \
            ARCH=arm64 \
            HOST_CROSS_COMPILE="${CROSS_COMPILE_ENV}" \
            TA_CROSS_COMPILE="${CROSS_COMPILE_ENV}" \
            TA_DEV_KIT_DIR="${TA_DEV_KIT_DIR}" \
            TA_SIGN_KEY="/opteeos_images/eve-optee.pem" \
            TEEC_EXPORT="${OPTEE_CLIENT_EXPORT}" ;\
         mkdir -p dist/lib/optee_armtz dist/usr/lib/tee-supplicant/plugins dist/usr/bin ;\
         cp -v out/ca/* dist/usr/bin/ ;\
         cp -v out/ta/*.ta dist/lib/optee_armtz/ ;\
         cp -v out/plugins/*.plugin dist/usr/lib/tee-supplicant/plugins/ ;\
         (cd dist/ ; tar -cvf "/tee-tarballs/${t}_examples.tar" ./)) ;\
         mkdir -p "${t}_dist" ;\
         tar -xvf "/tee-tarballs/${t}_client.tar" -C "${t}_dist/" ;\
         tar -xvf "/tee-tarballs/${t}_xtest.tar" -C "${t}_dist/" ;\
         tar -xvf "/tee-tarballs/${t}_examples.tar" -C "${t}_dist/" ;\
         (cd "${t}_dist" ; tar -cvf "/tee-dist/${t}_tee.tar" ./) ;\
    done

FROM scratch
COPY --from=build /tee-dist/ /tee-dist

