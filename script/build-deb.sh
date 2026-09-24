#!/bin/bash
# Compile TinySerial and build a Debian package for this Ubuntu release.
# Usage: ./script/build-deb.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ -n "${QT_QMAKE:-}" ]; then
    QMAKE="$QT_QMAKE"
elif command -v qmake >/dev/null 2>&1; then
    QMAKE="$(command -v qmake)"
elif [ -x /usr/lib/qt5/bin/qmake ]; then
    QMAKE=/usr/lib/qt5/bin/qmake
else
    echo "qmake was not found. Install the Qt 5 build packages:" >&2
    echo "  sudo apt-get install qt5-qmake qtbase5-dev libqt5serialport5-dev" >&2
    exit 1
fi

# shellcheck disable=SC1091
. /etc/os-release
ARCH="$(dpkg --print-architecture)"
VERSION="$(awk -F': ' '/^Version:/{print $2; exit}' tinyserial-deb-prj/DEBIAN/control)"
if [ -z "$VERSION" ]; then
    echo "Version is missing from tinyserial-deb-prj/DEBIAN/control" >&2
    exit 1
fi

BUILD_DIR="$ROOT/build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Compiling with $QMAKE"
(
    cd "$BUILD_DIR"
    "$QMAKE" "$ROOT/SerialPort.pro"
    make -j"${MAKE_JOBS:-$(nproc)}"
)

install -m 755 "$BUILD_DIR/tinyserial" "$ROOT/tinyserial-deb-prj/opt/tinyserial/tinyserial"
"$ROOT/gen_deb.sh"

mkdir -p "$ROOT/dist"
BASE_VERSION="${VERSION%%+*}"
DISTRO_TAG="${DISTRO_TAG:-ubuntu${VERSION_ID}}"
OUTPUT="$ROOT/dist/tinyserial_${BASE_VERSION}_${DISTRO_TAG}_${ARCH}.deb"
cp -a "$ROOT/tinyserial.deb" "$OUTPUT"
echo "Package: $OUTPUT"
