# Build a TinySerial .deb inside one Ubuntu release.
# The package version gains +ubuntuX.Y so each release can live in the same apt pool.

ARG UBUNTU_VERSION=22.04
FROM ubuntu:${UBUNTU_VERSION} AS build

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
COPY . .

RUN sed -i "s/^Version: .*/Version: ${PKG_VERSION}+ubuntu${UBUNTU_VERSION}/" tinyserial-deb-prj/DEBIAN/control
ENV MAKE_JOBS=${MAKE_JOBS}
ENV DISTRO_TAG=ubuntu${UBUNTU_VERSION}
RUN ./script/build-deb.sh

FROM scratch AS package
COPY --from=build /src/dist/ /
