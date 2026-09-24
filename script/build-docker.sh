#!/bin/bash
# Build TinySerial .deb packages for several Ubuntu releases.
# On tensor1 the BuildKit state is stored under /hrom/tinyserial, away from
# the system Docker data used by the OTA and LLM containers.
# Usage: ./script/build-docker.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(awk -F': ' '/^Version:/{print $2; exit}' tinyserial-deb-prj/DEBIAN/control)"
VERSION="${VERSION%%+*}"
RELEASES=(20.04 22.04 24.04 26.04)
JOBS="${MAKE_JOBS:-8}"

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
docker buildx use tinyserial

for release in "${RELEASES[@]}"; do
    echo "Building Ubuntu ${release}"
    dest="$STORE/dist/${release}"
    rm -rf "$dest"
    mkdir -p "$dest"
    docker buildx build \
        --builder tinyserial \
        --target package \
        --build-arg "UBUNTU_VERSION=${release}" \
        --build-arg "PKG_VERSION=${VERSION}" \
        --build-arg "MAKE_JOBS=${JOBS}" \
        --output "type=local,dest=${dest}" \
        -f docker/Dockerfile \
        "$ROOT"
    cp -a "$dest"/*.deb "$ROOT/dist/"
done

echo "Packages:"
ls -lh "$ROOT/dist"/*.deb
