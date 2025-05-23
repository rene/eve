# syntax=docker/dockerfile-upstream:1.5.0-rc2-labs

FROM lfedge/eve-alpine:0f2e0da38e30753c68410727a6cc269e57ff74f2 as grub-build-base
ENV BUILD_PKGS automake \
               make \
               bison \
               gettext \
               flex \
               gcc \
               git \
               libtool \
               libc-dev \
               linux-headers \
               python3 \
               autoconf \
               pkgconf \
               patch \
               gettext-dev \
               bash \
               gawk
ENV BUILD_PKGS_arm64 coreutils
ENV BUILD_PKGS_riscv64 coreutils
RUN eve-alpine-deploy.sh

# list of grub modules that are portable between x86_64 and aarch64
ENV GRUB_MODULES_PORT="part_gpt fat ext2 iso9660 squash4 gzio linux acpi normal cpio crypto disk boot crc64 \
search_disk_uuid search_part_label search_label xzio xfs video gfxterm serial gptprio chain probe reboot regexp smbios \
part_msdos cat echo test configfile loopback net tftp http true"

FROM grub-build-base AS grub-build-amd64
ENV GRUB_MODULES="multiboot multiboot2 efi_uga efi_gop linuxefi gpt verify gcry_sha256 measurefs efinet"
ENV GRUB_MODULES_i386_pc="multiboot multiboot2 biosdisk gpt verify"
ENV GRUB_COMMIT=71f9e4ac44142af52c3fc1860436cf9e432bf764
ENV GRUB_PATCHES=patches
# colon separates a platform from an additional option.
# this is far from perfect, but it works for now.
ENV GRUB_PLATFORM="efi pc:--disable-efiemu"

FROM grub-build-base AS grub-build-arm64
ENV GRUB_MODULES="xen_boot efi_gop gpt gcry_sha256 measurefs efinet"
ENV GRUB_COMMIT=2.06
ENV GRUB_PATCHES="patches-${GRUB_COMMIT} patches-aarch64-${GRUB_COMMIT}"
ENV GRUB_PLATFORM=efi

FROM grub-build-base AS grub-build-riscv64
ENV GRUB_MODULES="efinet"
ENV GRUB_COMMIT=2.06
ENV GRUB_PATCHES="patches-${GRUB_COMMIT} patches-riscv64-${GRUB_COMMIT}"
ENV GRUB_PLATFORM=efi

# hadolint ignore=DL3006
FROM grub-build-${TARGETARCH} AS grub-build

ENV GRUB_REPO=http://10.208.13.10

COPY patches /patches
COPY patches-2.06  /patches-2.06
COPY patches-aarch64-2.06 /patches-aarch64-2.06
COPY patches-riscv64-2.06 /patches-riscv64-2.06
# because python is not available
RUN ln -s python3 /usr/bin/python && \
    mkdir /grub-lib

ADD ${GRUB_REPO}/snapshot/grub-${GRUB_COMMIT}.tar.gz /grub.tar.gz
ENV GNULIB_REVISION=d271f868a8df9bbec29049d01e056481b7a1a263
ADD --keep-git-dir https://github.com/coreutils/gnulib.git#${GNULIB_REVISION} /gnulib

# the below does a weird init of a git repo, because we are not cloning the
# repo, yet we need to be in a repo to apply patches with `git am`.
# hadolint ignore=DL3003,SC2086
RUN if [ ! -d "grub" ]; then \
        tar -xzf /grub.tar.gz && \
        mv "grub-${GRUB_COMMIT}" grub && \
        rm -f /grub.tar.gz && \
        cd grub && \
        mkdir -p /apply/patches && \
        for dir in ${GRUB_PATCHES}; do \
            cp -r /${dir}/* /apply/patches; \
        done && \
        git config --global user.name "Your Name" && \
        git config --global user.email "you@example.com" && \
        git init . && git add . && git commit -m "init" && \
        git am /apply/patches/*; \
    fi

WORKDIR /grub

RUN ./bootstrap --gnulib-srcdir=/gnulib || ./autogen.sh

# hadolint ignore=SC2046
RUN set -e; for p in ${GRUB_PLATFORM}; do \
      platform=${p%%:*}; \
      begin=$(( ${#platform} + 1 )); \
      opt=${p:${begin}}; \
      if [ -f Makefile ]; then make distclean; fi; \
      ./configure --disable-werror --libdir=/grub-lib --with-platform="${platform}" ${opt} CFLAGS='-Os -Wno-unused-value'; \
      make -j $(getconf _NPROCESSORS_ONLN); \
      make install; \
    done

COPY embedded.cfg /

# create the grub core image
# hadolint ignore=SC2086
RUN case "$(uname -m)" in \
  x86_64) \
    ./grub-mkimage -O x86_64-efi -d /grub-lib/grub/x86_64-efi -o /grub-lib/BOOTX64.EFI -p /EFI/BOOT ${GRUB_MODULES_PORT} ${GRUB_MODULES} &&\
    ./grub-mkimage -O i386-pc -d /grub-lib/grub/i386-pc -o /grub-lib/BOOT.pc -p /EFI/BOOT -c /embedded.cfg ${GRUB_MODULES_PORT} ${GRUB_MODULES_i386_pc} && \
    cp /grub-lib/grub/i386-pc/boot.img /grub-lib/BOOT.img ;;\
  aarch64) \
    ./grub-mkimage -O arm64-efi -d /grub-lib/grub/arm64-efi -o /grub-lib/BOOTAA64.EFI -p /EFI/BOOT ${GRUB_MODULES_PORT} ${GRUB_MODULES} ;; \
  riscv64) \
     ./grub-mkimage -O riscv64-efi -d /grub-lib/grub/riscv64-efi -o /grub-lib/BOOTRISCV64.EFI -p /EFI/BOOT ${GRUB_MODULES_PORT} ${GRUB_MODULES} ;;\
  esac

FROM scratch
ENTRYPOINT []
CMD []
WORKDIR /EFI/BOOT
COPY --from=grub-build /grub-lib/BOOT* ./
COPY rootfs.cfg grub.cfg
