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
#pragma comment(lib, "shell32.lib")

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

// 带可选主动查询的设备获取（用于提高“在线/可连接”检测的及时性）
vector<BluetoothDeviceInfo> GetPairedDevicesWithInquiry(bool doInquiry) {
    vector<BluetoothDeviceInfo> devices;

    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { 0 };
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fReturnUnknown = FALSE;
    searchParams.fIssueInquiry = doInquiry ? TRUE : FALSE; // 主动扫描
    searchParams.cTimeoutMultiplier = doInquiry ? 2 : 1;   // 适当延长一点扫描时间

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

// 检查设备是否可达（在线）- 更严格的检测
bool IsDeviceAvailable(const BLUETOOTH_ADDRESS& address) {
    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    // 查询设备信息
    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    // 如果设备已连接，肯定在线
    if (deviceInfo.fConnected) {
        return true;
    }
    
    // 对于未连接的设备，不再默认认为在线
    // 只在主动扫描时检测到设备才认为在线
    return false;
}

// 辅助：错误码转字符串
wstring Win32ErrorToString(DWORD code) {
    LPWSTR buf = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, (LPWSTR)&buf, 0, NULL);
    wstring msg = len ? wstring(buf) : L"";
    if (buf) LocalFree(buf);
    return msg;
}

// 辅助：GUID 转字符串
wstring GuidToString(const GUID& g) {
    wchar_t buf[64];
    swprintf_s(buf, 64, L"{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        g.Data1, g.Data2, g.Data3,
        g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return wstring(buf);
}

// 辅助：打开第一个蓝牙无线电句柄
HANDLE OpenFirstRadio() {
    BLUETOOTH_FIND_RADIO_PARAMS params = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    HBLUETOOTH_RADIO_FIND hFind = nullptr;
    HANDLE hRadio = nullptr;
    hFind = BluetoothFindFirstRadio(&params, &hRadio);
    if (hFind != NULL) {
        BluetoothFindRadioClose(hFind);
        return hRadio;
    }
    return nullptr;
}

// 辅助：判断服务是否为音频相关
bool IsAudioService(const GUID& guid) {
    // Audio Source, Audio Sink, A/V Remote Control, Handsfree, Headset
    return (guid == AudioSinkServiceClass_UUID ||
            guid == AudioSourceServiceClass_UUID ||
            guid == AVRemoteControlTargetServiceClass_UUID ||
            guid == AVRemoteControlServiceClass_UUID ||
            guid == HandsfreeServiceClass_UUID ||
            guid == HeadsetServiceClass_UUID);
}

// 辅助：判断服务是否为 GATT 服务
bool IsGATTService(const GUID& guid) {
    // Generic Access (0x1800), Generic Attribute (0x1801)
    return (guid.Data1 == 0x1800 || guid.Data1 == 0x1801) &&
           guid.Data2 == 0x0000 && guid.Data3 == 0x1000;
}

// 连接蓝牙设备（参考提供的代码：通过禁用/启用音频相关服务触发连接）
bool ConnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    wcout << L"尝试连接设备: " << deviceName << L" [" << BluetoothAddressToString(address) << L"]" << endl;

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    // 获取设备信息
    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        wcout << L"  获取设备信息失败: " << result << L" " << Win32ErrorToString(result) << endl;
        return false;
    }

    if (deviceInfo.fConnected) {
        wcout << L"  设备已连接" << endl;
        return true;
    }

    // 打开本地蓝牙无线电
    HANDLE hRadio = OpenFirstRadio();
    if (!hRadio) {
        wcout << L"  未找到蓝牙适配器" << endl;
        return false;
    }

    // 优先从“已安装服务”中过滤目标服务，减少 1060/87 错误
    vector<GUID> installed;
    {
        const DWORD CAP = 32;
        GUID guids[CAP] = {};
        DWORD returned = CAP;
        DWORD er = BluetoothEnumerateInstalledServices(hRadio, &deviceInfo, &returned, guids);
        if (er == ERROR_SUCCESS && returned > 0) {
            for (DWORD i = 0; i < returned; ++i) installed.push_back(guids[i]);
        }
    }

    auto contains = [](const vector<GUID>& vec, const GUID& g) {
        for (const auto& x : vec) if (x == g) return true; return false;
    };

    // 参考实现：先禁用再启用常见音频相关服务（按优先级）
    vector<GUID> wanted = {
        AudioSinkServiceClass_UUID,                 // A2DP 音频接收器
        AudioSourceServiceClass_UUID,               // A2DP 音频源
        HandsfreeServiceClass_UUID,                 // 免提
        HeadsetServiceClass_UUID,                   // 头戴式/耳机
        AVRemoteControlTargetServiceClass_UUID,     // AVRCP 目标
        AVRemoteControlServiceClass_UUID            // AVRCP 控制器
    };

    vector<GUID> services;
    if (!installed.empty()) {
        for (const auto& g : wanted) if (contains(installed, g)) services.push_back(g);
    }
    if (services.empty()) services = wanted; // 无法枚举时按全量尝试

    bool anySuccess = false;
    for (const auto& svc : services) {
        // 先禁用
        BluetoothSetServiceState(hRadio, &deviceInfo, &svc, BLUETOOTH_SERVICE_DISABLE);
        Sleep(150);

        // 再启用（先用 hRadio，87 时回退到 NULL）
        DWORD r = BluetoothSetServiceState(hRadio, &deviceInfo, &svc, BLUETOOTH_SERVICE_ENABLE);
        if (r == ERROR_INVALID_PARAMETER) {
            r = BluetoothSetServiceState(NULL, &deviceInfo, &svc, BLUETOOTH_SERVICE_ENABLE);
        }
        if (r == ERROR_SUCCESS) {
            anySuccess = true;
            wcout << L"  成功启用服务: " << GuidToString(svc) << endl;
            // 给系统一些时间建立链路
            Sleep(1200);

            // 检查是否已连接
            DWORD r2 = BluetoothGetDeviceInfo(NULL, &deviceInfo);
            if (r2 == ERROR_SUCCESS && deviceInfo.fConnected) {
                wcout << L"  连接成功" << endl;
                CloseHandle(hRadio);
                return true;
            }
        } else if (r == ERROR_SERVICE_DOES_NOT_EXIST) {
            // 跳过未安装的服务，减少噪声
        } else {
            wcout << L"  启用服务失败: " << r << L" " << Win32ErrorToString(r) << endl;
        }
    }

    // 最终再检查一次连接状态
    DWORD r3 = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (r3 == ERROR_SUCCESS && deviceInfo.fConnected) {
        wcout << L"  连接成功" << endl;
        CloseHandle(hRadio);
        return true;
    }

    CloseHandle(hRadio);
    wcout << L"  连接失败" << endl;
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

// 名称匹配：patterns 中任意一项作为子串出现在 name 中即认为匹配（与示例代码一致）
bool MatchAnySubstring(const wstring& name, const set<wstring>& patterns) {
    if (patterns.empty()) return true;
    for (const auto& p : patterns) {
        if (!p.empty() && name.find(p) != wstring::npos) return true;
    }
    return false;
}

// 监听并自动连接设备
void MonitorAndConnect() {
    wcout << L"=== 蓝牙设备自动连接程序 ===" << endl;
    wcout << L"正在扫描已配对的蓝牙设备..." << endl << endl;

    // 读取配置文件
    set<wstring> monitorDevices = LoadConfig(L"config.txt");
    
    // 获取已配对设备列表（首次主动扫描以刷新在线状态）
    wcout << L"正在执行蓝牙设备扫描..." << endl;
    vector<BluetoothDeviceInfo> pairedDevices = GetPairedDevicesWithInquiry(true);
    wcout << L"扫描完成" << endl;
    
    if (pairedDevices.empty()) {
        wcout << L"未找到已配对的蓝牙设备。" << endl;
        wcout << L"请先在系统设置中配对蓝牙设备。" << endl;
        return;
    }

    wcout << L"找到 " << pairedDevices.size() << L" 个已配对的设备:" << endl;
    
    // 筛选要监控的设备
    vector<BluetoothDeviceInfo> devicesToMonitor;
    for (const auto& device : pairedDevices) {
        bool shouldMonitor = MatchAnySubstring(device.name, monitorDevices);
        
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
    int scanCount = 0;  // 主动扫描次数
    
    while (true) {
        checkCount++;
        
        // 每 3 次检查做一次主动扫描（更频繁地扫描以发现新设备）
        bool doInquiry = (checkCount % 3) == 0;
        if (doInquiry) {
            scanCount++;
            wcout << L"[" << checkCount << L"] 执行主动扫描 #" << scanCount << L"..." << endl;
        }
        
        // 获取当前设备状态
        vector<BluetoothDeviceInfo> currentDevices = GetPairedDevicesWithInquiry(doInquiry);
        
        // 检查每个要监控的设备
        for (size_t i = 0; i < devicesToMonitor.size(); i++) {
            const auto& pairedDevice = devicesToMonitor[i];
            bool currentlyConnected = false;
            bool deviceFound = false;
            
            // 查找当前状态
            for (const auto& current : currentDevices) {
                if (memcmp(&current.address, &pairedDevice.address, sizeof(BLUETOOTH_ADDRESS)) == 0) {
                    currentlyConnected = current.connected;
                    deviceFound = true;
                    break;
                }
            }
            
            // 如枟扫描中找不到设备，跳过
            if (!deviceFound) {
                continue;
            }
            
            // 检测状态变化
            if (currentlyConnected && !lastConnectedState[i]) {
                // 设备已连接
                wcout << L"[" << checkCount << L"] ✅ 设备已连接: " << pairedDevice.name << endl;
                lastConnectedState[i] = true;
            }
            else if (!currentlyConnected && lastConnectedState[i]) {
                // 二次确认，避免误判（列表状态可能短暂不同步）
                BLUETOOTH_DEVICE_INFO di = {0};
                di.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
                di.Address = pairedDevice.address;
                DWORD rchk = BluetoothGetDeviceInfo(NULL, &di);
                if (rchk == ERROR_SUCCESS && di.fConnected) {
                    // 仍然连接，跳过
                } else {
                    wcout << L"[" << checkCount << L"] ❌ 设备已断开: " << pairedDevice.name << endl;
                    lastConnectedState[i] = false;
                }
            }
            else if (!currentlyConnected && doInquiry) {
                // 在主动扫描时发现设备未连接，尝试连接
                wcout << L"[" << checkCount << L"] 🔍 发现设备未连接，尝试连接: " << pairedDevice.name << endl;
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
