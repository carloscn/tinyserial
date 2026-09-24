# Ubuntu toolchain for one TinySerial build environment.
# script/build-docker.sh exports this once, then later compiles only run make.
# MAKE_JOBS and PKG_VERSION are unused. They stay above the apt step so a
# BuildKit cache from the previous package builds still matches.

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
