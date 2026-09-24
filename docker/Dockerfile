# The toolchain stage is built once per Ubuntu release and then reused.
# PKG_VERSION is not installed. script/build-docker.sh always passes 1.5.3 so
# this layer keeps hitting the cache that is already on disk. The package
# version is PACKAGE_VERSION in the next stage, which does not rebuild Qt.

ARG UBUNTU_VERSION=22.04
FROM ubuntu:${UBUNTU_VERSION} AS toolchain

ENV DEBIAN_FRONTEND=noninteractive
ARG MAKE_JOBS=8
ARG PKG_VERSION=1.5.1
ARG UBUNTU_VERSION

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        dpkg-dev \
        libqt5serialport5-dev \
        python3 \
        qt5-qmake \
        qtbase5-dev \
        qtbase5-dev-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

FROM toolchain AS build
ARG PACKAGE_VERSION=1.5.3
ARG UBUNTU_VERSION
ARG COMPILE_JOBS=8
COPY . .
RUN sed -i "s/^Version: .*/Version: ${PACKAGE_VERSION}+ubuntu${UBUNTU_VERSION}/" tinyserial-deb-prj/DEBIAN/control
ENV MAKE_JOBS=${COMPILE_JOBS}
ENV DISTRO_TAG=ubuntu${UBUNTU_VERSION}
RUN ./script/build-deb.sh

FROM scratch AS package
COPY --from=build /src/dist/ /
