# TinySerial

A small, fast serial port terminal for talking to microcontrollers, boards and modems over UART. Built with Qt 5 or Qt 6.

Website: https://tinyserial.mltz.tech

![TinySerial on Ubuntu, light theme](img/tinyserial-light.png)

## Features

- Port list refreshes when a USB-serial adapter is plugged in or removed
- ASCII or HEX, chosen separately for sending and receiving
- Repeat sending at a fixed interval in milliseconds
- Optional timestamps and echo of sent data
- Save the session to a log file, or log to a file continuously while it runs
- Standard or custom baud rates, data bits, parity, stop bits and flow control
- Light and dark themes; English, Chinese and Spanish interface

![TinySerial on Ubuntu, dark theme](img/tinyserial-dark.png)

## Install on Ubuntu

Packages are published for Ubuntu 20.04, 22.04, 24.04 and 26.04 (amd64). Paste this block; it picks the apt suite for your release and adds you to the `dialout` group:

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

Log out and back in once so the `dialout` group applies, then open TinySerial from the application menu or run `tinyserial`.

## Build from source

With qmake and Qt 5:

```bash
sudo apt-get update
sudo apt-get install -y build-essential qt5-qmake qtbase5-dev libqt5serialport5-dev
git clone https://github.com/carloscn/tinyserial.git
cd tinyserial
qmake SerialPort.pro
make -j"$(nproc)"
./tinyserial
```

With CMake, which uses Qt 6 when it is installed and Qt 5 otherwise:

```bash
sudo apt-get install -y build-essential cmake ninja-build qtbase5-dev libqt5serialport5-dev
cmake -S . -B build-cmake -G Ninja -DBUILD_TESTING=ON
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
./build-cmake/tinyserial
```

Windows and macOS are not packaged; build them the same way with qmake or CMake.

Requirements: Qt 5.12 or newer, and permission to open serial devices (`dialout` on Ubuntu).

## Usage

1. Pick the port. The list follows hot-plug events; Refresh rescans at once.
2. Set baud rate, data bits, stop bits, parity and flow control, then Connect.
3. Type in the send box, choose ASCII or HEX, and press Send, or enable Repeat send.
4. Received data appears in the Terminal tab. Pause stops the display; data keeps arriving.
5. File > Save Log File exports the session.

Real-time logging writes the traffic to a file as it arrives. Packaged Linux installs use `/var/log/tinyserial`; other builds fall back to the user's application data directory when that is not writable.

## Troubleshooting

**TinySerial is missing from the application menu.** Reinstall it; packages older than 1.5.2 removed the menu entry while upgrading:

```bash
sudo apt-get update
sudo apt-get install -y --reinstall tinyserial
```

**No serial ports, or permission denied.** Add yourself to `dialout`, then log out and back in. The install block already does this.

```bash
sudo usermod -a -G dialout "$USER"
```

**It does not start.** Install it with apt so the Qt libraries for your Ubuntu release are present, and check that `/opt/tinyserial/tinyserial` exists.

**HEX input is rejected.** Bytes may be separated by spaces, but each byte needs exactly two hex digits, for example `01 AB FF`.

More package details: [tinyserial-deb-prj/README.md](tinyserial-deb-prj/README.md).

## Packaging and releases

Build a `.deb` for the Ubuntu you are on (output: `dist/tinyserial_<version>_ubuntu<release>_amd64.deb`):

```bash
sudo apt-get install -y qt5-qmake qtbase5-dev libqt5serialport5-dev dpkg-dev
./script/build-deb.sh
```

Build packages for all four Ubuntu releases with Docker. The Qt toolchain for each release is cached; on a machine where `/hrom` is writable the cache is `/hrom/tinyserial/buildkit`.

```bash
./script/build-docker.sh
```

Publish: raise `Version` in `tinyserial-deb-prj/DEBIAN/control`, then run `./script/release.sh` (needs a logged-in `gh`). It uploads `dist/tinyserial_*_ubuntu*.deb` to a GitHub Release, and the release workflow copies them to apt.mltz.tech.

`gen_deb.sh` fills the package `Depends` line from the binary on the build machine. Qt already depends on X11, zlib and GLib, so those are not listed again.

## Roadmap

- Data plotting (the Plotting tab is a placeholder for now)
- Better custom baud rate support
- macOS polish
- Scripted sending

## Contributing

Bug reports and pull requests are welcome: https://github.com/carloscn/tinyserial/issues

Related: [tinyNetwork](https://github.com/carloscn/tinyNetwork), a network debugging tool by the same author.

## License

Mozilla Public License 2.0. See [LICENSE](LICENSE).

Carlos Wei · carlos.wei.hk@gmail.com · https://mltz.tech
