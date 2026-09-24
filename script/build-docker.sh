#!/bin/bash
# Compile TinySerial packages for several Ubuntu releases.
# Each Ubuntu toolchain is cached under /hrom/tinyserial/buildkit on tensor1.
# A later run reuses that toolchain and only compiles. The toolchain is built
# again only when docker/Dockerfile's package install changes.
# Usage: ./script/build-docker.sh
#        ONLY_RELEASES="26.04" ./script/build-docker.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(awk -F': ' '/^Version:/{print $2; exit}' tinyserial-deb-prj/DEBIAN/control)"
VERSION="${VERSION%%+*}"
if [ -n "${ONLY_RELEASES:-}" ]; then
    # shellcheck disable=SC2206
    RELEASES=($ONLY_RELEASES)
else
    RELEASES=(20.04 22.04 24.04 26.04)
fi
JOBS="${MAKE_JOBS:-8}"
# Fixed cache key for the toolchain stage. Do not tie this to the package version.
TOOLCHAIN_CACHE_KEY=1.5.3

if [ -d /hrom ] && [ -w /hrom ]; then
    STORE=/hrom/tinyserial
else
    STORE="$ROOT/.docker-store"
fi
mkdir -p "$STORE/buildkit" "$STORE/dist" "$ROOT/dist"

if ! docker info >/dev/null 2>&1; then
    echo "Docker is not available for this user." >&2
    exit 1
fi

if ! docker ps --filter name=tinyserial-buildkit --filter status=running -q | grep -q .; then
    docker rm -f tinyserial-buildkit >/dev/null 2>&1 || true
    docker run -d --name tinyserial-buildkit --privileged --restart no \
        -v "$STORE/buildkit:/var/lib/buildkit" \
        -p 127.0.0.1:12345:1234 \
        moby/buildkit:latest \
        --addr tcp://0.0.0.0:1234
fi

if ! docker buildx inspect tinyserial >/dev/null 2>&1; then
    docker buildx create --name tinyserial --driver remote tcp://127.0.0.1:12345
fi

for release in "${RELEASES[@]}"; do
    echo "Compiling Ubuntu ${release} with the cached toolchain"
    dest="$STORE/dist/${release}"
    rm -rf "$dest"
    mkdir -p "$dest"
    docker buildx build \
        --builder tinyserial \
        --target package \
        --build-arg "UBUNTU_VERSION=${release}" \
        --build-arg "PKG_VERSION=${TOOLCHAIN_CACHE_KEY}" \
        --build-arg "PACKAGE_VERSION=${VERSION}" \
        --build-arg "COMPILE_JOBS=${JOBS}" \
        --output "type=local,dest=${dest}" \
        -f "$ROOT/docker/Dockerfile" \
        "$ROOT"
    cp -a "$dest"/*.deb "$ROOT/dist/"
done

echo "Packages:"
ls -lh "$ROOT/dist"/tinyserial_"${VERSION}"_ubuntu*.deb
