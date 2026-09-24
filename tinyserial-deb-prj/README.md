# TinySerial Debian Package Build Guide

## Prerequisites

Before building the Debian package, ensure you have the required tools installed:

```bash
sudo apt-get update
sudo apt-get install dpkg-dev build-essential
```

## Building the Package

1. **Prepare the package structure:**
   - Ensure all application files are in the correct location under `tinyserial-deb-prj/`
   - The binary should be placed in the appropriate directory structure

2. **Build the package:**
   ```bash
   cd /path/to/tinyserial
   chmod +x gen_deb.sh
   ./gen_deb.sh
   ```

   This will create `tinyserial.deb` in the project root directory.

## Installing the Package

1. **Install the package:**
   ```bash
   sudo dpkg -i tinyserial.deb
   ```

2. **If you encounter dependency issues:**
   ```bash
   sudo apt-get install -f
   ```
   This will automatically install any missing dependencies.

3. **Verify installation:**
   ```bash
   tinyserial
   ```
   Or open TinySerial from the application menu. The menu entry uses the application icon and does not open a terminal.

## Uninstalling the Package

```bash
sudo dpkg -r tinyserial
```

Or using apt:

```bash
sudo apt-get remove tinyserial
```

## Package Structure

```
tinyserial-deb-prj/
├── DEBIAN/
│   ├── control      # Package metadata and dependencies
│   ├── postinst     # Refresh desktop and icon caches
│   ├── postrm       # Refresh caches after removal
│   ├── prerm        # Pre-removal script
│   ├── changlog     # Package changelog
│   └── copyright    # Copyright information
├── opt/tinyserial/tinyserial
├── usr/bin/tinyserial -> /opt/tinyserial/tinyserial
├── usr/share/applications/tinyserial.desktop
├── usr/share/pixmaps/tinyserial.png
└── usr/share/icons/hicolor/72x72/apps/tinyserial.png
```

## Troubleshooting

### Dependency Issues

Build the `.deb` on the Ubuntu release you want to support. `gen_deb.sh` runs `dpkg-shlibdeps` and writes `Depends` from the binary just compiled on that system. Ubuntu 24.04 and 26.04 use different package names for the same Qt libraries, so one hand-written list cannot cover both.

Install with apt so those generated dependencies are pulled in:

```bash
sudo apt-get install ./tinyserial.deb
```

### Permission Issues

If the application doesn't run:

1. **Check file permissions:**
   ```bash
   ls -l /opt/tinyserial/tinyserial
   ```

2. **Fix permissions if needed:**
   ```bash
   sudo chmod 755 /opt/tinyserial/tinyserial
   ```

### Desktop Entry Issues

If the application doesn't appear in the applications menu:

1. **Update desktop database:**
   ```bash
   sudo update-desktop-database /usr/share/applications
   ```

2. **Check desktop file:**
   ```bash
   cat /usr/share/applications/tinyserial.desktop
   ```

## Package Information

- **Package Name:** tinyserial
- **Version:** 1.5.1
- **Architecture:** amd64
- **Maintainer:** Carlos Wei <carlos.wei.hk@gmail.com>
- **Homepage:** https://github.com/carloscn/tinyserial

## Dependencies

`DEBIAN/control` keeps `Depends: @SHLIBS@`. `gen_deb.sh` replaces that marker with the libraries this binary actually links, using the package names of the machine where the package is built. Qt already depends on X11, zlib, and GLib, so those are not listed again.

## Notes

- The package installs files to `/opt/tinyserial`
- A symlink is created at `/usr/bin/tinyserial` for command-line access
- Desktop entry is installed to `/usr/share/applications/`
- Icon is installed to `/usr/share/pixmaps/` and `/usr/share/icons/hicolor/72x72/apps/`

