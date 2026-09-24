#!/bin/bash
# Compile TinySerial packages for several Ubuntu releases.
# The compiler, Qt, and Debian tools are exported once to $STORE/roots/<release>.
# Later runs only compile. On tensor1 that directory is /hrom/tinyserial/roots.
# Usage: ./script/build-docker.sh

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
HELPER="${HELPER_IMAGE:-ubuntu:22.04}"
DOCKERFILE_HASH="$(sha256sum "$ROOT/docker/Dockerfile" | awk '{print $1}')"

if [ -d /hrom ] && [ -w /hrom ]; then
    STORE=/hrom/tinyserial
else
    STORE="$ROOT/.docker-store"
fi
mkdir -p "$STORE/buildkit" "$STORE/roots" "$ROOT/dist"

if ! docker info >/dev/null 2>&1; then
    echo "Docker is not available for this user." >&2
    exit 1
fi

ensure_buildkit() {
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
}

ensure_helper() {
    if ! docker image inspect "$HELPER" >/dev/null 2>&1; then
        docker pull "$HELPER"
    fi
}

prepare_toolchain() {
    local release=$1
    local root="$STORE/roots/${release}"
    local stamp="$STORE/roots/${release}.stamp"
    if [ -x "$root/usr/bin/qmake" ] && [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$DOCKERFILE_HASH" ]; then
        echo "Ubuntu ${release} toolchain is ready"
        return
    fi
    echo "Preparing Ubuntu ${release} toolchain once at ${root}"
    ensure_buildkit
    rm -rf "$root"
    mkdir -p "$root"
    docker buildx build \
        --builder tinyserial \
        --target toolchain \
        --build-arg "UBUNTU_VERSION=${release}" \
        --build-arg "PKG_VERSION=${VERSION}" \
        --output "type=local,dest=${root}" \
        -f "$ROOT/docker/Dockerfile" \
        "$ROOT"
    printf '%s\n' "$DOCKERFILE_HASH" > "$stamp"
}

compile_release() {
    local release=$1
    local root="$STORE/roots/${release}"
    echo "Compiling Ubuntu ${release}"
    sed -i "s/^Version: .*/Version: ${VERSION}+ubuntu${release}/" "$ROOT/tinyserial-deb-prj/DEBIAN/control"
    docker run --rm --privileged \
        -e MAKE_JOBS="$JOBS" \
        -e DISTRO_TAG="ubuntu${release}" \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        -v "$root":/rootfs \
        -v "$ROOT":/rootfs/src \
        "$HELPER" \
        bash -c 'mount -t proc proc /rootfs/proc && mount --bind /dev /rootfs/dev && chroot /rootfs setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups --inh-caps=-all bash -lc "cd /src && ./script/build-deb.sh"'
}

CONTROL_BACKUP="$(mktemp)"
cp -a "$ROOT/tinyserial-deb-prj/DEBIAN/control" "$CONTROL_BACKUP"
restore_control() {
    cp -a "$CONTROL_BACKUP" "$ROOT/tinyserial-deb-prj/DEBIAN/control"
    rm -f "$CONTROL_BACKUP"
}
trap restore_control EXIT

ensure_helper
for release in "${RELEASES[@]}"; do
    prepare_toolchain "$release"
    compile_release "$release"
done

echo "Packages:"
ls -lh "$ROOT/dist"/tinyserial_"${VERSION}"_ubuntu*.deb
