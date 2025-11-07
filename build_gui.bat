@echo off
echo === 蓝牙监听程序 GUI 版本编译脚本 ===
echo.

REM 检查是否安装了 Visual Studio
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo 未找到 Visual Studio 编译器 cl.exe
    echo 请运行 Visual Studio Developer Command Prompt
    echo 或者先执行: "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    pause
    exit /b 1
)

echo 正在编译 GUI 版本...
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitorGUI.cpp ^
    /link Bthprops.lib ws2_32.lib comctl32.lib shell32.lib user32.lib ^
    /SUBSYSTEM:WINDOWS ^
    /OUT:BluetoothMonitorGUI.exe

if %errorlevel% equ 0 (
    echo.
    echo === 编译成功 ===
    echo 可执行文件: BluetoothMonitorGUI.exe
    echo.
    echo 运行方式:
    echo   BluetoothMonitorGUI.exe
    echo.
    echo 功能特性:
    echo   - GUI 窗口界面
    echo   - 系统托盘图标
    echo   - 实时设备状态显示
    echo   - 日志查看
    echo.
) else (
    echo.
    echo === 编译失败 ===
    echo 请检查错误信息
)

pause
