#include <windows.h>
#include <bluetoothapis.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <chrono>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 将 BLUETOOTH_ADDRESS 转换为字符串
wstring BluetoothAddressToString(const BLUETOOTH_ADDRESS& addr) {
    wchar_t buffer[18];
    swprintf_s(buffer, L"%02X:%02X:%02X:%02X:%02X:%02X",
        addr.rgBytes[5], addr.rgBytes[4], addr.rgBytes[3],
        addr.rgBytes[2], addr.rgBytes[1], addr.rgBytes[0]);
    return wstring(buffer);
}

// 蓝牙设备信息结构
struct BluetoothDeviceInfo {
    BLUETOOTH_ADDRESS address;
    wstring name;
    bool connected;
};

// 获取所有已配对的蓝牙设备
vector<BluetoothDeviceInfo> GetPairedDevices() {
    vector<BluetoothDeviceInfo> devices;
    
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { 0 };
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;  // 返回已配对的设备
    searchParams.fReturnRemembered = TRUE;     // 返回记住的设备
    searchParams.fReturnConnected = TRUE;      // 返回已连接的设备
    searchParams.fReturnUnknown = FALSE;
    searchParams.fIssueInquiry = FALSE;        // 不进行新的查询
    searchParams.cTimeoutMultiplier = 1;

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    
    if (hFind != NULL) {
        do {
            BluetoothDeviceInfo info;
            info.address = deviceInfo.Address;
            info.name = deviceInfo.szName;
            info.connected = deviceInfo.fConnected;
            devices.push_back(info);
        } while (BluetoothFindNextDevice(hFind, &deviceInfo));
        
        BluetoothFindDeviceClose(hFind);
    }
    
    return devices;
}

// 检查设备是否可达（在线）
bool IsDeviceAvailable(const BLUETOOTH_ADDRESS& address) {
    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    // 查询设备信息
    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    
    if (result == ERROR_SUCCESS) {
        // 检查设备的最后查看时间，判断是否在线
        SYSTEMTIME lastSeen = deviceInfo.stLastSeen;
        SYSTEMTIME now;
        GetSystemTime(&now);
        
        // 简单判断：如果最近被看到，认为设备在线
        return deviceInfo.fConnected || deviceInfo.fRemembered;
    }
    
    return false;
}

// 连接蓝牙设备
bool ConnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    wcout << L"尝试连接设备: " << deviceName << L" [" << BluetoothAddressToString(address) << L"]" << endl;

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    // 获取设备信息
    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        wcout << L"  获取设备信息失败: " << result << endl;
        return false;
    }

    // 如果已经连接，返回成功
    if (deviceInfo.fConnected) {
        wcout << L"  设备已连接" << endl;
        return true;
    }

    // 尝试通过设置服务状态来连接设备
    result = BluetoothSetServiceState(NULL, &deviceInfo, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);
    
    if (result == ERROR_SUCCESS) {
        // 再次检查是否真的连接成功
        this_thread::sleep_for(chrono::milliseconds(500));
        result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
        if (result == ERROR_SUCCESS && deviceInfo.fConnected) {
            wcout << L"  连接成功" << endl;
            return true;
        }
    }
    
    wcout << L"  连接失败，错误代码: " << result << endl;
    return false;
}

// 读取配置文件
set<wstring> LoadConfig(const wstring& configFile) {
    set<wstring> monitorDevices;
    wifstream file(configFile);
    
    if (!file.is_open()) {
        return monitorDevices;
    }
    
    wstring line;
    while (getline(file, line)) {
        // 去除空白和注释
        size_t commentPos = line.find(L'#');
        if (commentPos != wstring::npos) {
            line = line.substr(0, commentPos);
        }
        
        // 去除首尾空格
        size_t start = line.find_first_not_of(L" \t\r\n");
        size_t end = line.find_last_not_of(L" \t\r\n");
        
        if (start != wstring::npos && end != wstring::npos) {
            line = line.substr(start, end - start + 1);
            if (!line.empty()) {
                monitorDevices.insert(line);
            }
        }
    }
    
    file.close();
    return monitorDevices;
}

// 监听并自动连接设备
void MonitorAndConnect() {
    wcout << L"=== 蓝牙设备自动连接程序 ===" << endl;
    wcout << L"正在扫描已配对的蓝牙设备..." << endl << endl;

    // 读取配置文件
    set<wstring> monitorDevices = LoadConfig(L"config.txt");
    
    // 获取已配对设备列表
    vector<BluetoothDeviceInfo> pairedDevices = GetPairedDevices();
    
    if (pairedDevices.empty()) {
        wcout << L"未找到已配对的蓝牙设备。" << endl;
        wcout << L"请先在系统设置中配对蓝牙设备。" << endl;
        return;
    }

    wcout << L"找到 " << pairedDevices.size() << L" 个已配对的设备:" << endl;
    
    // 筛选要监控的设备
    vector<BluetoothDeviceInfo> devicesToMonitor;
    for (const auto& device : pairedDevices) {
        bool shouldMonitor = monitorDevices.empty() || monitorDevices.count(device.name) > 0;
        
        wcout << L"  - " << device.name << L" [" << BluetoothAddressToString(device.address) << L"]";
        wcout << (device.connected ? L" (已连接)" : L" (未连接)");
        
        if (shouldMonitor) {
            wcout << L" [监控中]";
            devicesToMonitor.push_back(device);
        }
        wcout << endl;
    }
    wcout << endl;
    
    if (devicesToMonitor.empty()) {
        wcout << L"没有需要监控的设备。" << endl;
        wcout << L"请在 config.txt 中配置设备名称，或删除 config.txt 以监控所有设备。" << endl;
        return;
    }

    wcout << L"开始监听设备状态..." << endl;
    wcout << L"按 Ctrl+C 停止监听" << endl << endl;

    // 记录上一次的连接状态
    vector<bool> lastConnectedState(devicesToMonitor.size(), false);
    for (size_t i = 0; i < devicesToMonitor.size(); i++) {
        lastConnectedState[i] = devicesToMonitor[i].connected;
    }

    // 持续监听循环
    int checkCount = 0;
    while (true) {
        checkCount++;
        
        // 获取当前设备状态
        vector<BluetoothDeviceInfo> currentDevices = GetPairedDevices();
        
        // 检查每个要监控的设备
        for (size_t i = 0; i < devicesToMonitor.size(); i++) {
            const auto& pairedDevice = devicesToMonitor[i];
            bool currentlyConnected = false;
            
            // 查找当前状态
            for (const auto& current : currentDevices) {
                if (memcmp(&current.address, &pairedDevice.address, sizeof(BLUETOOTH_ADDRESS)) == 0) {
                    currentlyConnected = current.connected;
                    break;
                }
            }
            
            // 检测状态变化
            if (currentlyConnected && !lastConnectedState[i]) {
                // 设备上线
                wcout << L"[" << checkCount << L"] 检测到设备上线: " << pairedDevice.name << endl;
                lastConnectedState[i] = true;
            }
            else if (!currentlyConnected && lastConnectedState[i]) {
                // 设备离线
                wcout << L"[" << checkCount << L"] 检测到设备离线: " << pairedDevice.name << endl;
                lastConnectedState[i] = false;
            }
            else if (!currentlyConnected) {
                // 设备未连接，尝试连接
                wcout << L"[" << checkCount << L"] 设备未连接，尝试建立连接: " << pairedDevice.name << endl;
                if (ConnectDevice(pairedDevice.address, pairedDevice.name)) {
                    lastConnectedState[i] = true;
                }
            }
        }
        
        // 每 5 秒检查一次
        this_thread::sleep_for(chrono::seconds(5));
    }
}

int main() {
    // 设置控制台输出为 UTF-16
    _setmode(_fileno(stdout), _O_U16TEXT);
    
    try {
        MonitorAndConnect();
    }
    catch (const exception& e) {
        cerr << "发生错误: " << e.what() << endl;
        return 1;
    }

    return 0;
}
