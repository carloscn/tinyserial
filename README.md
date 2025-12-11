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

### Installation

#### For Ubuntu/Debian Users

**Method 1: Using apt (Recommended - Auto-installs dependencies)**
```bash
sudo apt-get install ./tinyserial.deb
```

**Method 2: Using dpkg**
```bash
sudo dpkg -i tinyserial.deb
```

If you encounter dependency issues:
```bash
sudo apt-get install -f
```

**Note:** Version 1.5+ includes optimized dependency management. For detailed troubleshooting, see the [package documentation](tinyserial-deb-prj/README.md).

#### For Other Platforms

Download the source code and compile it yourself (see [Building from Source](#-building-from-source) section).

### Running TinySerial

After installation, you can launch TinySerial in one of the following ways:

1. **Command Line:**
   ```bash
   tinyserial
   ```

2. **Application Menu:**
   Find "TinySerial" in your system's application menu

3. **Direct Binary:**
   ```bash
   /opt/tinyserial/tinyserial
   ```

## 📋 System Requirements

- **Operating System**: Linux (Ubuntu 16.04+, Debian 9+), Windows 7+, macOS 10.12+
- **Qt Framework**: Qt 5.5 or higher
- **Dependencies**:
  - Qt5 Widgets, GUI, Core, SerialPort, DBus
  - Standard C/C++ runtime libraries
  - X11 libraries (Linux)

## 🔨 Building from Source

### Prerequisites

- Qt 5.5 or higher
- C++ compiler with C++11 support (GCC 4.8+, Clang 3.3+, MSVC 2015+)
- CMake or qmake
- Qt SerialPort module

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/carloscn/tinyserial.git
   cd tinyserial
   ```

2. **Build with qmake:**
   ```bash
   qmake SerialPort.pro
   make
   ```

   Or on Windows with MinGW:
   ```bash
   qmake SerialPort.pro
   mingw32-make
   ```

3. **Run the application:**
   ```bash
   ./tinyserial
   ```

## 📖 Usage Guide

### Basic Operations

1. **Scan for Serial Ports**: Click the "Scan" button to detect available serial ports
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

**Q: Application won't start**
- Ensure all Qt5 dependencies are installed
- Check file permissions: `chmod +x /opt/tinyserial/tinyserial`

**Q: Can't find serial ports**
- Verify your user has permission to access serial ports
- On Linux, you may need to add your user to the `dialout` group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
- Log out and log back in for changes to take effect

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

- [ ] Dynamic USB device detection
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
