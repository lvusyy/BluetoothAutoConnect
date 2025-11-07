@echo off
echo === 蓝牙监听程序编译脚本 ===
echo.

REM 检查是否安装了 Visual Studio
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo 未找到 Visual Studio 编译器 cl.exe
    echo 请运行 Visual Studio Developer Command Prompt
    echo 或者先执行: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    pause
    exit /b 1
)

echo 正在编译...
cl.exe /EHsc /std:c++17 /utf-8 /D_UNICODE /DUNICODE BluetoothMonitor.cpp ^
    /link Bthprops.lib ws2_32.lib shell32.lib ^
    /OUT:BluetoothMonitor.exe

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
