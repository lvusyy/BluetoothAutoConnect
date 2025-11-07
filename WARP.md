# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

A Windows Bluetooth device auto-connection tool that monitors paired devices and automatically connects them when they come online. Available in both console and GUI versions.

- **Language**: C++17
- **Platform**: Windows 10/11 only
- **Dependencies**: Windows Bluetooth API (bluetoothapis.h)
- **Build System**: Visual Studio compiler (cl.exe) with batch scripts, CMake optional

## Build Commands

**Prerequisites**: Must run from Visual Studio Developer Command Prompt or after running vcvars64.bat

### Console Version
```cmd
build.bat
```
Manual compilation:
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitor.cpp /link Bthprops.lib ws2_32.lib /OUT:BluetoothMonitor.exe
```

### GUI Version
```cmd
build_gui.bat
```
Manual compilation:
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitorGUI.cpp /link Bthprops.lib ws2_32.lib comctl32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS /OUT:BluetoothMonitorGUI.exe
```

### CMake (Alternative)
```cmd
cmake -B build
cmake --build build
```

## Running the Application

**Console version**: `BluetoothMonitor.exe`
**GUI version**: `BluetoothMonitorGUI.exe`

Both versions require paired Bluetooth devices in Windows settings before running.

## Architecture

### Core Components

**BluetoothMonitor.cpp** - Console version
- Main loop with 5-second polling interval
- Direct console output with wcout
- Ctrl+C to exit

**BluetoothMonitorGUI.cpp** - GUI version
- Win32 GUI with system tray integration
- Separate monitoring thread with mutex-protected logging
- ListView for device status, Edit control for logs
- Tray icon with context menu (show/hide/config/exit)

### Shared Logic Pattern

Both versions implement the same core workflow:

1. **Device Discovery**: `GetPairedDevices()` - Enumerates all paired devices using `BluetoothFindFirstDevice/BluetoothFindNextDevice`
2. **Config Filtering**: `LoadConfig(L"config.txt")` - Loads device whitelist from config file
3. **Connection Logic**: `ConnectDevice()` - Uses `BluetoothSetServiceState()` with `HumanInterfaceDeviceServiceClass_UUID`
4. **Status Monitoring**: Polls `BluetoothGetDeviceInfo()` to check `fConnected` flag every 5 seconds

### Key Windows APIs Used

- `BluetoothFindFirstDevice/BluetoothFindNextDevice` - Device enumeration
- `BluetoothGetDeviceInfo` - Query connection status
- `BluetoothSetServiceState` - Initiate connection with `HumanInterfaceDeviceServiceClass_UUID`
- `BluetoothAuthenticateDeviceEx` - Device pairing/authentication (not actively used in current implementation)

### Configuration System

`config.txt` format:
- One device name per line (case-sensitive, must match Windows device name exactly)
- Lines starting with `#` are comments
- Empty/missing file = monitor all paired devices
- Device filtering happens in memory after enumeration

## Code Style

From CONTRIBUTING.md:
- 4 spaces indentation
- camelCase for variables
- PascalCase for functions
- UPPER_CASE for constants
- Semantic commit messages: `feat:`, `fix:`, `docs:`, `style:`, `refactor:`, `test:`, `chore:`

## Project Context

- All UI text is in Chinese (Simplified)
- Uses UTF-8 encoding (`/utf-8` compiler flag)
- Unicode build (`/D_UNICODE /DUNICODE`)
- Designed for always-on background monitoring
- GUI version has system tray for minimized operation
- No automatic tests - manual testing with real Bluetooth devices required
