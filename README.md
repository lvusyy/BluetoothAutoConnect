# 蓝牙设备自动连接监听程序

## 项目简介

这是一个 Windows 平台的蓝牙设备自动连接工具，用于持续监听已配对的蓝牙设备，当检测到设备上线（在场）时主动建立连接，并在断开后继续监听并重连。支持所有类型的蓝牙设备，包括蓝牙音频设备（耳机、音箱等）。

提供 **控制台版本** 和 **GUI 版本** 两种选择。

## 功能特性

### 通用功能
- ✅ 自动扫描所有已配对的蓝牙设备
- ✅ 实时监听设备上线/离线状态
- ✅ 自动连接未连接的设备
- ✅ 断线后自动重连
- ✅ 支持蓝牙音频设备（耳机、音箱）
- ✅ 支持所有蓝牙设备类型
- ✅ 配置文件支持，可指定监控设备

### GUI 版本额外功能
- ✅ 友好的图形界面
- ✅ 系统托盘图标，最小化到托盘
- ✅ 实时设备状态列表显示
- ✅ 日志实时显示和清空
- ✅ 可直接打开配置文件编辑
- ✅ 双击托盘图标恢复窗口

## 系统要求

- **操作系统**: Windows 10/11
- **编译器**: 以下任一编译器
  - Visual Studio 2019/2022 (推荐)
  - MinGW-w64
  - MSYS2 + GCC
  - CMake + 任意 C++ 编译器

## 编译方法

### 控制台版本

#### Visual Studio 命令行（推荐）

1. 打开 **Developer Command Prompt for VS 2022** (或 VS 2019)
2. 进入项目目录:
   ```cmd
   cd D:\codeBase\BluetoothAutoConnect
   ```
3. 运行编译脚本:
   ```cmd
   build.bat
   ```

### GUI 版本

#### Visual Studio 命令行（推荐）

1. 打开 **Developer Command Prompt for VS 2022** (或 VS 2019)
2. 进入项目目录:
   ```cmd
   cd D:\codeBase\BluetoothAutoConnect
   ```
3. 运行编译脚本:
   ```cmd
   build_gui.bat
   ```

#### 手动编译

**控制台版本:**
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitor.cpp /link Bthprops.lib ws2_32.lib /OUT:BluetoothMonitor.exe
```

**GUI 版本:**
```cmd
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitorGUI.cpp /link Bthprops.lib ws2_32.lib comctl32.lib shell32.lib /SUBSYSTEM:WINDOWS /OUT:BluetoothMonitorGUI.exe
```

## 使用方法

### 前提条件

1. 确保 Windows 蓝牙功能已启用
2. 在系统设置中先配对要监听的蓝牙设备
   - 打开"设置" → "蓝牙和设备" → "添加设备"
   - 配对你的蓝牙耳机、音箱或其他设备

### 运行程序

#### 控制台版本

1. 双击运行 `BluetoothMonitor.exe`
2. 或在命令行中运行:
   ```cmd
   BluetoothMonitor.exe
   ```

#### GUI 版本（推荐）

1. 双击运行 `BluetoothMonitorGUI.exe`
2. 程序会显示 GUI 窗口并自动开始监控
3. 可以最小化到系统托盘，后台运行
4. 右键托盘图标显示菜单：
   - 显示窗口
   - 打开配置文件
   - 退出
5. 双击托盘图标可快速显示窗口

### 程序行为

- 程序启动后会扫描所有已配对的蓝牙设备
- 显示设备列表及当前连接状态
- 每 5 秒检查一次设备状态
- 自动尝试连接未连接的设备
- 检测到设备上线/离线时会显示通知
- 按 `Ctrl+C` 停止程序

### 示例输出

```
=== 蓝牙设备自动连接程序 ===
正在扫描已配对的蓝牙设备...

找到 2 个已配对的设备:
  - AirPods Pro [A1:B2:C3:D4:E5:F6] (未连接)
  - Sony WH-1000XM4 [11:22:33:44:55:66] (已连接)

开始监听设备状态...
按 Ctrl+C 停止监听

[1] 设备未连接，尝试建立连接: AirPods Pro
尝试连接设备: AirPods Pro [A1:B2:C3:D4:E5:F6]
  连接成功
[2] 检测到设备离线: Sony WH-1000XM4
[3] 设备未连接，尝试建立连接: Sony WH-1000XM4
...
```

## 项目结构

```
BluetoothAutoConnect/
├── BluetoothMonitor.cpp     # 控制台版本源代码
├── BluetoothMonitorGUI.cpp  # GUI 版本源代码
├── config.txt                # 配置文件（可选）
├── build.bat                 # 控制台版编译脚本
├── build_gui.bat             # GUI 版编译脚本
├── resource.rc               # GUI 资源文件
├── resource.h                # 资源头文件
└── README.md                 # 项目说明文档
```

## 技术说明

### 使用的 Windows API

- **Bluetooth APIs** (`bluetoothapis.h`)
  - `BluetoothFindFirstDevice` / `BluetoothFindNextDevice`: 枚举蓝牙设备
  - `BluetoothGetDeviceInfo`: 获取设备详细信息
  - `BluetoothAuthenticateDeviceEx`: 连接/验证设备
  - `BluetoothFindDeviceClose`: 关闭设备句柄

### 核心功能实现

1. **设备扫描**: 使用 `BluetoothFindFirstDevice` 枚举所有已配对设备
2. **状态监听**: 定期轮询设备连接状态
3. **自动连接**: 检测到未连接设备时调用 `BluetoothAuthenticateDeviceEx`
4. **断线重连**: 检测状态变化，自动尝试重新连接

## 注意事项

1. **权限要求**: 程序需要有蓝牙访问权限，首次运行可能需要管理员权限
2. **设备配对**: 设备必须先在系统设置中配对，程序才能检测和连接
3. **音频设备**: 对于蓝牙音频设备，连接成功后 Windows 会自动将其设为音频输出设备
4. **连接限制**: 某些蓝牙设备可能有连接限制或特殊配对要求
5. **检查间隔**: 默认每 5 秒检查一次，可在代码中修改 `chrono::seconds(5)` 调整

## 自定义配置

### 配置要监控的设备

在项目目录下创建 `config.txt` 文件，每行填写一个要监控的设备名称：

```txt
# 蓝牙设备自动连接配置文件
# 每行填写一个要监控的设备名称
# 使用 # 开头的行为注释

WH-1000XM5
AirPods Pro
```

**说明**：
- 如果 `config.txt` 不存在或为空，程序将监控**所有**已配对的设备
- 使用 `#` 开头的行为注释，会被程序忽略
- 设备名称必须与系统中显示的名称完全一致（区分大小写）
- 程序启动时会显示哪些设备正在被监控

### 修改检查间隔

在 `BluetoothMonitor.cpp` 最后的循环中:

```cpp
// 将 5 改为你想要的秒数
this_thread::sleep_for(chrono::seconds(5));
```

## 故障排除

### 编译错误

- **找不到 bluetoothapis.h**: 确保使用 Windows SDK
- **链接错误**: 确保链接了 `Bthprops.lib` 和 `ws2_32.lib`

### 运行错误

- **未找到设备**: 检查蓝牙是否开启，设备是否已配对
- **连接失败**: 确保设备在范围内且未被其他设备占用
- **权限错误**: 以管理员身份运行程序

### 连接不稳定

- 减小检查间隔（不建议低于 3 秒）
- 检查设备是否在有效范围内
- 检查 Windows 蓝牙驱动是否正常

## 开发环境

- **开发语言**: C++17
- **目标平台**: Windows 10/11 (x64)
- **依赖库**: Windows Bluetooth API

## 许可证

本项目仅供学习和个人使用。

## 贡献

欢迎提交 Issue 和 Pull Request。

## 更新日志

### v1.2 (2025-11-07)
- ✨ 新增 GUI 版本（`BluetoothMonitorGUI.exe`）
- ✨ 添加系统托盘功能，可后台运行
- ✨ 实时显示设备状态列表
- ✨ GUI 日志查看和清空功能
- ✨ 托盘菜单支持快速打开配置文件

### v1.1 (2025-11-07)
- 修复连接检测逻辑，使用 `BluetoothSetServiceState` 并验证实际连接状态
- 添加配置文件支持（`config.txt`），可指定要监控的设备
- 改进输出信息，显示哪些设备正在被监控
- 修复 UTF-8 编码问题

### v1.0 (2025-11-07)
- 初始版本
- 支持自动监听和连接已配对蓝牙设备
- 支持蓝牙音频设备
- 断线自动重连
