cmake_minimum_required(VERSION 3.15)
project(BluetoothAutoConnect)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(BluetoothMonitor BluetoothMonitor.cpp)

# 链接 Windows 蓝牙库
target_link_libraries(BluetoothMonitor PRIVATE Bthprops ws2_32)

# 设置 Windows 子系统为控制台
if(MSVC)
    set_target_properties(BluetoothMonitor PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()
