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
   Or find it in your applications menu.

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
│   ├── postinst     # Post-installation script
│   ├── postrm       # Post-removal script
│   ├── prerm        # Pre-removal script
│   ├── changlog     # Package changelog
│   └── copyright    # Copyright information
└── tinyserial/      # Application files
    └── ...
```

## Troubleshooting

### Dependency Issues

If you encounter dependency problems:

1. **Check installed Qt5 packages:**
   ```bash
   dpkg -l | grep qt5
   ```

2. **Install missing dependencies manually:**
   ```bash
   sudo apt-get install libqt5widgets5 libqt5gui5 libqt5core5a libqt5serialport5
   ```

3. **Use apt to fix dependencies:**
   ```bash
   sudo apt-get install -f
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
- **Version:** 1.5
- **Architecture:** amd64
- **Maintainer:** Carlos Wei <carlos.wei.hk@gmail.com>
- **Homepage:** https://github.com/carloscn/tinyserial

## Dependencies

The package requires the following runtime dependencies:

- Qt5 Widgets, GUI, Core, SerialPort, DBus libraries
- Standard C/C++ runtime libraries
- X11 libraries for GUI support
- GLib and other system libraries

See `DEBIAN/control` for the complete list of dependencies.

## Notes

- The package installs files to `/opt/tinyserial`
- A symlink is created at `/usr/bin/tinyserial` for command-line access
- Desktop entry is installed to `/usr/share/applications/`
- Icon is installed to `/usr/share/pixmaps/`

