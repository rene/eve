FROM lfedge/eve-ubuntu:0a291748ed6118508d76df22a2bb24234de66bde AS ubuntu
FROM ubuntu AS build

ENV PKGS="curl"
ENV BUILD_PKGS="gcc openssh-client"

RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror sh -x /bin/eve-ubuntu-deploy.sh
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out /usr/bin/apt-get -y install openssh-client
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out apt-get update
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out apt-get -y install curl

FROM scratch
COPY --from=build /out/ /
