#!/bin/bash
# TinySerial Debian Package Builder
# This script builds a Debian package for TinySerial

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$SCRIPT_DIR/tinyserial-deb-prj"
OUTPUT_FILE="$SCRIPT_DIR/tinyserial.deb"

echo "=========================================="
echo "TinySerial Debian Package Builder"
echo "=========================================="

# Check if DEBIAN directory exists
if [ ! -d "$DEB_DIR/DEBIAN" ]; then
    echo "Error: DEBIAN directory not found in $DEB_DIR"
    exit 1
fi

# Check if control file exists
if [ ! -f "$DEB_DIR/DEBIAN/control" ]; then
    echo "Error: control file not found in $DEB_DIR/DEBIAN"
    exit 1
fi

# Make scripts executable
echo "Setting script permissions..."
chmod +x "$DEB_DIR/DEBIAN/postinst" 2>/dev/null || true
chmod +x "$DEB_DIR/DEBIAN/postrm" 2>/dev/null || true
chmod +x "$DEB_DIR/DEBIAN/prerm" 2>/dev/null || true

for required in \
    "$DEB_DIR/opt/tinyserial/tinyserial" \
    "$DEB_DIR/usr/bin/tinyserial" \
    "$DEB_DIR/usr/share/applications/tinyserial.desktop" \
    "$DEB_DIR/usr/share/pixmaps/tinyserial.png" \
    "$DEB_DIR/usr/share/icons/hicolor/72x72/apps/tinyserial.png"
do
    if [ ! -e "$required" ]; then
        echo "Error: required package file not found: $required"
        exit 1
    fi
done

if command -v desktop-file-validate >/dev/null 2>&1; then
    echo "Validating desktop entry..."
    desktop-file-validate "$DEB_DIR/usr/share/applications/tinyserial.desktop"
fi

# Check for required tools
if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "Error: dpkg-deb not found. Please install dpkg-dev package:"
    echo "  sudo apt-get install dpkg-dev"
    exit 1
fi

# Validate control file format
echo "Validating control file..."
if ! grep -q "^Package:" "$DEB_DIR/DEBIAN/control"; then
    echo "Error: Invalid control file format (missing Package field)"
    exit 1
fi

if ! grep -q "^Version:" "$DEB_DIR/DEBIAN/control"; then
    echo "Error: Invalid control file format (missing Version field)"
    exit 1
fi

if ! command -v dpkg-shlibdeps >/dev/null 2>&1; then
    echo "Error: dpkg-shlibdeps not found. Please install dpkg-dev:"
    echo "  sudo apt-get install dpkg-dev"
    exit 1
fi

# Build from a staging copy so the source tree keeps the @SHLIBS@ marker,
# and so the package README is not installed as /README.md.
STAGE="$(mktemp -d)"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT
cp -a "$DEB_DIR"/. "$STAGE/"
rm -f "$STAGE/README.md"
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm" "$STAGE/DEBIAN/prerm" \
    "$STAGE/opt/tinyserial/tinyserial"

echo "Resolving shared library dependencies for this system..."
SHLIB_ROOT="$(mktemp -d)"
mkdir -p "$SHLIB_ROOT/debian"
{
    echo "Source: tinyserial"
    cat "$STAGE/DEBIAN/control"
} > "$SHLIB_ROOT/debian/control"
SHLIBS_LINE="$(
    cd "$SHLIB_ROOT"
    dpkg-shlibdeps -O "$STAGE/opt/tinyserial/tinyserial"
)"
rm -rf "$SHLIB_ROOT"
SHLIBS_DEPS="${SHLIBS_LINE#shlibs:Depends=}"
if [ -z "$SHLIBS_DEPS" ] || [ "$SHLIBS_DEPS" = "$SHLIBS_LINE" ]; then
    echo "Error: dpkg-shlibdeps did not report shlibs:Depends"
    exit 1
fi
echo "Depends: $SHLIBS_DEPS"

python3 - "$STAGE/DEBIAN/control" "$SHLIBS_DEPS" <<'PY'
import pathlib, sys
path, deps = sys.argv[1:]
file = pathlib.Path(path)
file.write_text(file.read_text().replace("Depends: @SHLIBS@", "Depends: " + deps, 1))
PY

if grep -q '@SHLIBS@' "$STAGE/DEBIAN/control"; then
    echo "Error: failed to substitute shared library dependencies"
    exit 1
fi

# Build the package
echo "Building Debian package..."
DPKG_DEB_ARGS=()
if dpkg-deb --help 2>&1 | grep -q -- '--root-owner-group'; then
    DPKG_DEB_ARGS+=(--root-owner-group)
fi

if dpkg-deb "${DPKG_DEB_ARGS[@]}" --build "$STAGE" "$OUTPUT_FILE"; then
    echo ""
    echo "=========================================="
    echo "Package built successfully!"
    echo "Output: $OUTPUT_FILE"
    echo "=========================================="
    
    # Show package info
    echo ""
    echo "Package information:"
    dpkg-deb -I "$OUTPUT_FILE"
    
    echo ""
    echo "To install the package and its libraries, run:"
    echo "  sudo apt-get install $OUTPUT_FILE"
else
    echo "Error: Failed to build package"
    exit 1
fi
