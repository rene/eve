ARG GOVER=1.20.1
FROM golang:${GOVER}-alpine
ARG USER
ARG GROUP
ARG UID
ARG GID
# all_proxy is the standard proxy definer for socks proxies. Docker build only has built-ins for http_proxy,https_proxy,ftp_proxy,no_proxy
# so we need to declare it explicitly
# this must be an ARG so it doesn't carry through post-build phase
ARG all_proxy
# hadolint ignore=DL3018
RUN apk add --no-cache openssh-client git gcc linux-headers libc-dev util-linux libpcap-dev bash vim make protobuf \
        protobuf-dev sudo tar curl graphviz ttf-freefont patch dnsmasq ca-certificates build-base gettext-dev \
        libtirpc-dev automake autoconf libtool attr-dev e2fsprogs-dev glib-dev openssl-dev util-linux-dev

RUN deluser ${USER} ; delgroup ${GROUP} || :
RUN sed -ie /:${UID}:/d /etc/passwd /etc/shadow ; sed -ie /:${GID}:/d /etc/group || :
RUN addgroup -g ${GID} ${GROUP} && adduser -h /home/${USER} -G ${GROUP} -D -H -u ${UID} ${USER}
RUN echo "${USER} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USER}
### Build OpenZFS 2.2.2 libs here for 'make test' and full eve build in one spot
# should be aligned with kernel
#  * ZFS on Linux
# coreutils's uname -o breaks above url generation.
# hadolint ignore=DL3018
RUN apk add --no-cache coreutils
ENV ZFS_VERSION=2.2.2
ENV ZFS_COMMIT=zfs-${ZFS_VERSION}
ENV ZFS_REPO=https://github.com/openzfs/zfs

WORKDIR /tmp/zfs
# hadolint ignore=DL3020
ADD ${ZFS_REPO}/tarball/${ZFS_COMMIT}/ zfs.tgz
RUN tar -zxvf zfs.tgz  --strip-components=1 && \
    rm zfs.tgz

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
    make DESTDIR=/tmp/zfs-out install-strip

# cleanup
RUN rm -rf /tmp/zfs-out/usr/share && rm -rf /tmp/zfs-out/usr/src && \
    rm -rf /tmp/zfs-out/etc/init.d && rm -rf /tmp/zfs-out/etc/conf.d

RUN cp -r /tmp/zfs-out/* /

COPY domain.crt /tmp/domain.crt

RUN mkdir -p /etc/docker/certs.d/10.208.13.132
RUN cp /tmp/domain.crt /etc/docker/certs.d/10.208.13.132/ca.crt
RUN cp /tmp/domain.crt /usr/local/share/ca-certificates/selfs.crt
RUN update-ca-certificates

RUN go install github.com/golang/protobuf/protoc-gen-go@v1.5.2
RUN go install gotest.tools/gotestsum@v1.7.0
RUN go install github.com/seamia/protodot@87817c3d0a8e7af753af15508b51292e941bc7c6
RUN mv /go/bin/protodot /usr/local/bin
RUN mv /go/bin/* /usr/bin
ENV HOME /home/${USER}
ENV GOFLAGS=-mod=vendor
ENV GO111MODULE=on
ENV REGISTRY=${REGISTRY}
