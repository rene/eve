# Copyright (c) 2026 Zededa, Inc.
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:26.04 AS builder

ENV PKGS="build-essential \
	gawk \
	autoconf \
	psmisc \
	iproute2 \
	passwd \
	vim \
	git \
	cmake \
	curl \
	wget \
	jq \
	lsof \
	gdb \
	openssh-client \
	openssh-server \
	make"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get -y install apt-file dpkg-dev strace
RUN apt-file update

RUN mkdir -p /mirror/archives

RUN apt-get -y install debootstrap
WORKDIR /root
RUN debootstrap --cache-dir=/mirror/archives --variant=minbase --arch=amd64 resolute /out

RUN chroot /out apt-get update
RUN chroot /out apt-get purge --allow-remove-essential -y procps
# findutils sed libbsd0 sensible-utils are still needed to install further packages
RUN chroot /out apt-get purge --allow-remove-essential -y gawk login login.defs bsdutils readline-common libsemanage2 mount bash gzip e2fsprogs login.defs ncurses-base ncurses-bin libext2fs2t64 sysvinit-utils procps logsave passwd
RUN rm -Rf /out/var/cache/apt/archives /out/usr/share/coreutils/locales
RUN rm -f /out/usr/share/locale/*/LC_MESSAGES/*.mo
RUN rm -Rf /out/usr/share/man /out/usr/share/doc
RUN rm -Rf /out/usr/share/bash-completion /out/usr/share/fish /out/usr/share/zsh

COPY recurse-debs.pl /root
RUN for i in ${PKGS}; do apt-get -yd install --reinstall -o dir::cache=/mirror $(/root/recurse-debs.pl $i); done
WORKDIR /mirror/archives
RUN dpkg-scanpackages -m . | gzip -c > Packages.gz

RUN echo "deb [trusted=yes] file:///mirror/archives/ ./" > /etc/apt/sources.list
RUN echo "deb [trusted=yes] file:///mirror/archives/ ./" > /out/etc/apt/sources.list
RUN rm -Rvf /etc/apt/sources.list.d
RUN rm -Rvf /out/etc/apt/sources.list.d
RUN apt-get update

FROM builder
RUN --mount=type=bind,from=builder,source=/mirror,target=/out/mirror chroot /out /usr/bin/apt-get update

COPY eve-ubuntu-deploy.sh register-sbom-pkg.sh /bin/
