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

# Build the package
echo "Building Debian package..."
if dpkg-deb --build "$DEB_DIR" "$OUTPUT_FILE"; then
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
    echo "To install the package, run:"
    echo "  sudo dpkg -i $OUTPUT_FILE"
    echo ""
    echo "If you encounter dependency issues, run:"
    echo "  sudo apt-get install -f"
else
    echo "Error: Failed to build package"
    exit 1
fi
