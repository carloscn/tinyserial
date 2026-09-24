# TinySerial

<div align="center">

![TinySerial Logo](img/appmain-ubuntu.png)

**A cross-platform GUI serial port debug tool built with Qt5**

[![License](https://img.shields.io/badge/license-MPL--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)](https://github.com/carloscn/tinyserial)
[![Qt](https://img.shields.io/badge/Qt-5.15%2B-green.svg)](https://www.qt.io/)

</div>

## 📖 Overview

TinySerial is an open-source, cross-platform serial port debugging tool designed for developers and engineers who need to communicate with embedded devices, microcontrollers, and other serial devices. Built with Qt5 framework, it provides a user-friendly graphical interface for serial communication testing and debugging.

### ✨ Key Features

- 🔌 **Serial Port Management**: Automatic port scanning and easy connection
- 📊 **Dual Format Support**: ASCII and HEX data transmission modes
- 🔄 **Auto Repeat**: Configurable automatic data sending
- 📝 **Data Logging**: Save communication logs to files
- ⏱️ **Timestamp Display**: Optional timestamp for each message
- 🎨 **Modern UI**: Clean and intuitive user interface
- 🌐 **Cross-Platform**: Supports Linux and Windows
- 🔧 **Flexible Configuration**: Customizable baud rates, parity, stop bits, and flow control

## 🖼️ Screenshots

### Ubuntu/Linux Interface

![Ubuntu Interface](img/appmain-ubuntu.png)

### macOS Interface

![macOS Interface](img/appmain-mac.png)

## 🚀 Quick Start

### Install

Paste this whole block on Ubuntu 20.04, 22.04, 24.04, or 26.04. It picks the matching apt suite. Then log out and back in, and open **TinySerial** from the application menu.

```bash
(
  . /etc/os-release
  case "$VERSION_ID" in
    20.04) suite=focal ;;
    22.04) suite=jammy ;;
    24.04) suite=noble ;;
    26.04) suite=resolute ;;
    *) echo "Ubuntu ${VERSION_ID:-unknown} is not a published TinySerial target." >&2; exit 1 ;;
  esac
  sudo curl -fsSL https://apt.mltz.tech/key.gpg -o /usr/share/keyrings/mltz.gpg
  echo "deb [signed-by=/usr/share/keyrings/mltz.gpg] https://apt.mltz.tech ${suite} main" | sudo tee /etc/apt/sources.list.d/mltz.list
  sudo apt-get update
  sudo apt-get install -y tinyserial
  sudo usermod -a -G dialout "$USER"
)
```

The menu entry does not open a terminal. You can also run `tinyserial`.

### Compile the source on Ubuntu

Paste this whole block. It installs the Qt 5 build packages, compiles TinySerial, and runs it.

```bash
sudo apt-get update
sudo apt-get install -y build-essential qt5-qmake qtbase5-dev libqt5serialport5-dev
git clone https://github.com/carloscn/tinyserial.git
cd tinyserial
qmake SerialPort.pro
make -j"$(nproc)"
sudo usermod -a -G dialout "$USER"
./tinyserial
```

Log out and back in before opening a serial port, so the `dialout` group applies.

### Other platforms

Windows and macOS builds are not published to the apt repository. Clone the repository and build with qmake:

```bash
git clone https://github.com/carloscn/tinyserial.git
cd tinyserial
qmake SerialPort.pro
make
./tinyserial
```

## 📋 System Requirements

- **Published packages:** Ubuntu 20.04, 22.04, 24.04, and 26.04 (amd64)
- **Qt:** 5.12 or newer. The packages use the Qt libraries shipped by that Ubuntu release
- **Also required:** permission to open serial devices (`dialout` on Ubuntu)

`gen_deb.sh` fills the package `Depends` line from the binary on the machine where the package is built. Qt already depends on X11, zlib, and GLib, so those are not listed again.

## 🔨 Building and publishing

### Package for this Ubuntu

```bash
sudo apt-get install -y qt5-qmake qtbase5-dev libqt5serialport5-dev dpkg-dev
git clone https://github.com/carloscn/tinyserial.git
cd tinyserial
./script/build-deb.sh
```

The package is `dist/tinyserial_<version>_ubuntu<release>_amd64.deb`.

### Package Ubuntu 20.04, 22.04, 24.04, and 26.04

Docker is required. The Qt toolchain for each release is cached and reused. On a machine where `/hrom` is writable, that cache is `/hrom/tinyserial/buildkit`.

```bash
git clone https://github.com/carloscn/tinyserial.git
cd tinyserial
./script/build-docker.sh
```

### Publish those packages

`gh` must already be logged in. This uploads `dist/tinyserial_*_ubuntu*.deb` to a GitHub Release. The release workflow copies each file to `apt.mltz.tech`. Raise `Version` in `tinyserial-deb-prj/DEBIAN/control` before a new release.

```bash
./script/release.sh
```

## 📖 Usage Guide

### Basic Operations

1. **Scan for Serial Ports**: The list updates when a USB serial device is plugged or unplugged. Scan refreshes it immediately.
2. **Configure Port Settings**: Select baud rate, data bits, stop bits, parity, and flow control
3. **Connect**: Click "Open" to establish connection
4. **Send Data**: 
   - Enter data in the send text area
   - Choose ASCII or HEX format
   - Click "Send" or enable auto-repeat
5. **View Received Data**: Received data appears in the receive area
6. **Save Logs**: Use "Save Log File" from the menu to export communication logs

### Advanced Features

- **Auto Repeat**: Enable repeat sending with configurable interval
- **Display Options**: Toggle timestamp and send/receive indicators
- **Pause Output**: Temporarily pause receiving data display
- **Custom Baud Rate**: Enter custom baud rate values

## 🛠️ Troubleshooting

### Common Issues

**Q: TinySerial does not appear in the application menu**

```bash
sudo apt-get update
sudo apt-get install -y --reinstall tinyserial
```

Search for TinySerial in the application grid. A running session picks the new entry up within a few seconds. Packages older than 1.5.2 removed the menu entry while upgrading.

**Q: Application won't start**
- Install it with apt so the Qt libraries for that Ubuntu release are installed
- Check the program: `ls -l /opt/tinyserial/tinyserial`

**Q: Can't find serial ports**

```bash
sudo usermod -a -G dialout "$USER"
```

Log out and back in. The install block above already runs this command.

**Q: Permission denied errors**
- Run with appropriate permissions or configure udev rules for persistent access

**Q: Dependency issues on Linux**
- See the [package troubleshooting guide](tinyserial-deb-prj/README.md)

## 🤝 Contributing

Contributions are welcome! If you find bugs or have feature requests, please:

1. Open an [issue](https://github.com/carloscn/tinyserial/issues) on GitHub
2. Fork the repository and create a feature branch
3. Submit a pull request with your changes

## 📝 TODO

- [x] Dynamic USB device detection
- [ ] Enhanced custom baud rate support
- [ ] macOS version optimization
- [ ] Data visualization and plotting
- [ ] Script automation support

## 🔗 Related Projects

- [tinyNetwork](https://github.com/carloscn/tinyNetwork) - Network debugging tool by the same author

## 📄 License

This project is licensed under the Mozilla Public License 2.0 (MPL-2.0). See the [LICENSE](LICENSE) file for details.

## 👤 Author

**Carlos Wei**

- Email: carlos.wei.hk@gmail.com
- GitHub: [@carloscn](https://github.com/carloscn)
- Homepage: http://www.mlts.tech/

## 🙏 Acknowledgments

- Built with [Qt Framework](https://www.qt.io/)
- Part of the MULTIBEANS project

## 📞 Support

If you encounter any issues or have questions:

- 📧 Email: carlos.wei.hk@gmail.com
- 🐛 [Report Issues](https://github.com/carloscn/tinyserial/issues)
- 📖 [Documentation](tinyserial-deb-prj/README.md)

---

<div align="center">

**Made with ❤️ by Carlos Wei**

⭐ Star this repo if you find it useful!

</div>
