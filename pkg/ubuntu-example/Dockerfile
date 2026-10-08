FROM lfedge/eve-ubuntu:8872c745c61eab5a8c7a4605791d7c96fb3cd20e AS ubuntu
FROM ubuntu AS build

ENV PKGS="curl"
ENV BUILD_PKGS="gcc openssh-client"

RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror sh -x /bin/eve-ubuntu-deploy.sh
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out /usr/bin/apt-get -y install openssh-client
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out apt-get update
#RUN --mount=type=bind,from=ubuntu,source=/mirror,target=/out/mirror chroot /out apt-get -y install curl

FROM scratch
COPY --from=build /out/ /
