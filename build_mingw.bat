@echo off
echo === 使用 MinGW 编译蓝牙监听程序 ===
echo.

REM 检查是否安装了 g++
where g++.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo 未找到 MinGW g++ 编译器
    echo 请先安装 MinGW 或 MSYS2
    pause
    exit /b 1
)

echo 正在编译...
g++ -std=c++17 -municode -DUNICODE -D_UNICODE BluetoothMonitor.cpp ^
    -o BluetoothMonitor.exe ^
    -lbthprops -lws2_32

if %errorlevel% equ 0 (
    echo.
    echo === 编译成功 ===
    echo 可执行文件: BluetoothMonitor.exe
    echo.
    echo 运行方式:
    echo   BluetoothMonitor.exe
    echo.
) else (
    echo.
    echo === 编译失败 ===
    echo 请检查错误信息
)

pause
