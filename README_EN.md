# Bluetooth Auto Connect Monitor

English | [中文](README.md)

## Project Overview

This is a Windows-based Bluetooth device auto-connect tool that continuously monitors paired Bluetooth devices. When a device is detected online (in range), it actively establishes a connection and continues to monitor and reconnect after disconnection. Supports all types of Bluetooth devices, including Bluetooth audio devices (headphones, speakers, etc.).

Two versions available: **Console Version** and **GUI Version**.

## Features

### Common Features
- ✅ Auto-scan all paired Bluetooth devices
- ✅ Real-time monitoring of device online/offline status
- ✅ Auto-connect disconnected devices
- ✅ Auto-reconnect after disconnection
- ✅ Support for Bluetooth audio devices (headphones, speakers)
- ✅ Support for all Bluetooth device types
- ✅ Configuration file support for specifying monitored devices

### GUI Version Additional Features
- ✅ User-friendly graphical interface
- ✅ System tray icon, minimize to tray
- ✅ Real-time device status list display
- ✅ Real-time log display and clear
- ✅ Direct access to edit configuration file
- ✅ Double-click tray icon to restore window

## System Requirements

- **Operating System**: Windows 10/11
- **Compiler**: Any of the following
  - Visual Studio 2019/2022 (Recommended)
  - MinGW-w64
  - MSYS2 + GCC
  - CMake + Any C++ compiler

## Build Instructions

### Console Version

#### Visual Studio Command Line (Recommended)

1. Open **Developer Command Prompt for VS 2022** (or VS 2019)
2. Navigate to project directory:
   ```cmd
   cd D:\codeBase\BluetoothAutoConnect
   ```
3. Run build script:
   ```cmd
   build.bat
   ```

### GUI Version

#### Visual Studio Command Line (Recommended)

1. Open **Developer Command Prompt for VS 2022** (or VS 2019)
2. Navigate to project directory:
   ```cmd
   cd D:\codeBase\BluetoothAutoConnect
   ```
3. Run build script:
   ```cmd
   build_gui.bat
   ```

#### Manual Build

**Console Version:**
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitor.cpp /link Bthprops.lib ws2_32.lib /OUT:BluetoothMonitor.exe
```

**GUI Version:**
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitorGUI.cpp /link Bthprops.lib ws2_32.lib comctl32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS /OUT:BluetoothMonitorGUI.exe
```

## Usage

### Prerequisites

1. Ensure Windows Bluetooth is enabled
2. Pair the Bluetooth devices you want to monitor in system settings first
   - Open "Settings" → "Bluetooth & devices" → "Add device"
   - Pair your Bluetooth headphones, speakers, or other devices

### Running the Program

#### Console Version

1. Double-click to run `BluetoothMonitor.exe`
2. Or run from command line:
   ```cmd
   BluetoothMonitor.exe
   ```

#### GUI Version (Recommended)

1. Double-click to run `BluetoothMonitorGUI.exe`
2. Program will display GUI window and automatically start monitoring
3. Can minimize to system tray for background operation
4. Right-click tray icon to show menu:
   - Show Window
   - Open Config File
   - Exit
5. Double-click tray icon to quickly show window

### Program Behavior

- After startup, scans all paired Bluetooth devices
- Displays device list and current connection status
- Checks device status every 5 seconds
- Automatically attempts to connect disconnected devices
- Shows notifications when devices go online/offline
- Press `Ctrl+C` to stop the program

### Example Output

```
=== Bluetooth Auto Connect Monitor ===
Scanning paired Bluetooth devices...

Found 2 paired devices:
  - AirPods Pro [A1:B2:C3:D4:E5:F6] (Disconnected)
  - Sony WH-1000XM4 [11:22:33:44:55:66] (Connected)

Starting device status monitoring...
Press Ctrl+C to stop

[1] Device disconnected, attempting connection: AirPods Pro
Attempting to connect device: AirPods Pro [A1:B2:C3:D4:E5:F6]
  Connection successful
[2] Device offline detected: Sony WH-1000XM4
[3] Device disconnected, attempting connection: Sony WH-1000XM4
...
```

## Project Structure

```
BluetoothAutoConnect/
├── BluetoothMonitor.cpp     # Console version source code
├── BluetoothMonitorGUI.cpp  # GUI version source code
├── config.txt                # Configuration file (optional)
├── build.bat                 # Console version build script
├── build_gui.bat             # GUI version build script
├── resource.rc               # GUI resource file
├── resource.h                # Resource header file
├── README.md                 # Chinese documentation
└── README_EN.md              # English documentation
```

## Technical Details

### Windows APIs Used

- **Bluetooth APIs** (`bluetoothapis.h`)
  - `BluetoothFindFirstDevice` / `BluetoothFindNextDevice`: Enumerate Bluetooth devices
  - `BluetoothGetDeviceInfo`: Get device details
  - `BluetoothAuthenticateDeviceEx`: Connect/authenticate device
  - `BluetoothFindDeviceClose`: Close device handle

### Core Implementation

1. **Device Scanning**: Use `BluetoothFindFirstDevice` to enumerate all paired devices
2. **Status Monitoring**: Periodically poll device connection status
3. **Auto Connect**: Call `BluetoothAuthenticateDeviceEx` when disconnected device is detected
4. **Auto Reconnect**: Detect status changes and automatically attempt reconnection

## Important Notes

1. **Permission Requirements**: Program needs Bluetooth access permission, may require administrator privileges on first run
2. **Device Pairing**: Devices must be paired in system settings before the program can detect and connect
3. **Audio Devices**: For Bluetooth audio devices, Windows will automatically set them as audio output after successful connection
4. **Connection Limits**: Some Bluetooth devices may have connection limits or special pairing requirements
5. **Check Interval**: Default check every 5 seconds, can modify `chrono::seconds(5)` in code to adjust

## Custom Configuration

### Configure Monitored Devices

Create a `config.txt` file in the project directory, with one device name per line:

```txt
# Bluetooth Auto Connect Configuration File
# One device name per line
# Lines starting with # are comments

WH-1000XM5
AirPods Pro
```

**Notes**:
- If `config.txt` doesn't exist or is empty, program will monitor **all** paired devices
- Lines starting with `#` are comments and will be ignored
- Device names must match exactly with system display names (case-sensitive)
- Program will show which devices are being monitored on startup

### Modify Check Interval

In the loop at the end of `BluetoothMonitor.cpp`:

```cpp
// Change 5 to your desired number of seconds
this_thread::sleep_for(chrono::seconds(5));
```

## Troubleshooting

### Build Errors

- **bluetoothapis.h not found**: Ensure Windows SDK is installed
- **Link errors**: Ensure `Bthprops.lib` and `ws2_32.lib` are linked

### Runtime Errors

- **No devices found**: Check if Bluetooth is enabled and devices are paired
- **Connection failed**: Ensure device is in range and not occupied by other devices
- **Permission error**: Run program as administrator

### Unstable Connection

- Reduce check interval (not recommended below 3 seconds)
- Check if device is within effective range
- Check if Windows Bluetooth driver is functioning properly

## Development Environment

- **Language**: C++17
- **Target Platform**: Windows 10/11 (x64)
- **Dependencies**: Windows Bluetooth API

## License

This project is for learning and personal use only.

## Contributing

Issues and Pull Requests are welcome.

## Changelog

### v1.3.2 (2025-11-07)
- 📝 Added English documentation
- 📝 Optimized documentation

### v1.2 (2025-11-07)
- ✨ Added GUI version (`BluetoothMonitorGUI.exe`)
- ✨ Added system tray functionality for background operation
- ✨ Real-time device status list display
- ✨ GUI log viewing and clearing functionality
- ✨ Tray menu quick access to configuration file

### v1.1 (2025-11-07)
- Fixed connection detection logic, using `BluetoothSetServiceState` and verifying actual connection status
- Added configuration file support (`config.txt`) to specify monitored devices
- Improved output information, showing which devices are being monitored
- Fixed UTF-8 encoding issues

### v1.0 (2025-11-07)
- Initial release
- Support for auto-monitoring and connecting paired Bluetooth devices
- Support for Bluetooth audio devices
- Auto-reconnect after disconnection
