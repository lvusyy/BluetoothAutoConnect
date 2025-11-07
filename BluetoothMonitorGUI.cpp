#include <windows.h>
#include <shellapi.h>
#include <bluetoothapis.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <fstream>
#include <codecvt>
#include <locale>
#include <unordered_map>

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;

// 窗口类名和标题
const wchar_t CLASS_NAME[] = L"BluetoothMonitorClass";
const wchar_t WINDOW_TITLE[] = L"蓝牙设备自动连接";

// 消息和控件ID
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_CONFIG 1003
#define ID_LOG_EDIT 2001
#define ID_DEVICE_LIST 2002
#define ID_BTN_START 2003
#define ID_BTN_STOP 2004
#define ID_BTN_CLEAR 2005
#define ID_DEVICE_CONNECT 3001
#define ID_DEVICE_DISCONNECT 3002
#define ID_DEVICE_COPY_MAC 3003
#define ID_DEVICE_REFRESH 3004
#define ID_DEVICE_COPY_NAME 3005
#define ID_DEVICE_ADD_MONITOR 3006
#define ID_DEVICE_REMOVE_MONITOR 3007

// 蓝牙设备信息结构
struct BluetoothDeviceInfo {
    BLUETOOTH_ADDRESS address;
    wstring name;
    bool connected;
};

// 全局变量
HINSTANCE g_hInst = nullptr;
HWND g_hwndMain = nullptr;
HWND g_hwndLog = nullptr;
HWND g_hwndDeviceList = nullptr;
NOTIFYICONDATAW g_nid = {};
bool g_bRunning = true;
mutex g_logMutex;
thread* g_pMonitorThread = nullptr;
vector<BluetoothDeviceInfo> g_currentDevices;
set<wstring> g_monitorDevices;
// 手动断开后，阻止自动重连的设备（按MAC字符串标识）
set<wstring> g_blockAutoReconnect;
// 每台设备的重连冷却时间戳
unordered_map<wstring, chrono::steady_clock::time_point> g_lastConnectAttempt;
static const int CONNECT_COOLDOWN_MS = 8000; // 8秒冷却

// 将 BLUETOOTH_ADDRESS 转换为字符串
wstring BluetoothAddressToString(const BLUETOOTH_ADDRESS& addr) {
    wchar_t buffer[18];
    swprintf_s(buffer, L"%02X:%02X:%02X:%02X:%02X:%02X",
        addr.rgBytes[5], addr.rgBytes[4], addr.rgBytes[3],
        addr.rgBytes[2], addr.rgBytes[1], addr.rgBytes[0]);
    return wstring(buffer);
}

// 添加日志
void AddLog(const wstring& message) {
    lock_guard<mutex> lock(g_logMutex);
    
    if (g_hwndLog) {
        int len = GetWindowTextLength(g_hwndLog);
        SendMessage(g_hwndLog, EM_SETSEL, len, len);
        SendMessage(g_hwndLog, EM_REPLACESEL, FALSE, (LPARAM)message.c_str());
        SendMessage(g_hwndLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        SendMessage(g_hwndLog, EM_SCROLLCARET, 0, 0);
    }
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
        size_t commentPos = line.find(L'#');
        if (commentPos != wstring::npos) {
            line = line.substr(0, commentPos);
        }
        
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

// 保存配置文件
bool SaveConfig(const wstring& configFile, const set<wstring>& monitorDevices) {
    // 使用 UTF-8 编码
    wofstream file(configFile, ios::out | ios::trunc);
    
    if (!file.is_open()) {
        return false;
    }
    
    // 设置 UTF-8 locale
    file.imbue(locale(locale(), new codecvt_utf8<wchar_t>));
    
    file << L"# 蓝牙设备自动连接配置文件\n";
    file << L"# 每行填写一个要监控的设备名称\n";
    file << L"# 使用 # 开头的行为注释\n";
    file << L"# 如果此文件为空或不存在，请右键添加设备到监控列表\n";
    file << L"\n";
    
    for (const auto& deviceName : monitorDevices) {
        file << deviceName << L"\n";
    }
    
    file.flush();
    file.close();
    
    return file.good() || !file.bad();
}

// 名称匹配：patterns 中任意一项作为子串出现在 text 中即认为匹配
bool MatchAnySubstring(const wstring& text, const set<wstring>& patterns) {
    if (patterns.empty()) return true;
    for (const auto& p : patterns) {
        if (!p.empty() && text.find(p) != wstring::npos) return true;
    }
    return false;
}

// 获取所有已配对的蓝牙设备
vector<BluetoothDeviceInfo> GetPairedDevices() {
    vector<BluetoothDeviceInfo> devices;
    
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { 0 };
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fReturnUnknown = FALSE;
    searchParams.fIssueInquiry = FALSE;
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
    searchParams.fIssueInquiry = doInquiry ? TRUE : FALSE;
    searchParams.cTimeoutMultiplier = doInquiry ? 2 : 1;

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
    return (guid == AudioSinkServiceClass_UUID ||
            guid == AudioSourceServiceClass_UUID ||
            guid == AVRemoteControlTargetServiceClass_UUID ||
            guid == AVRemoteControlServiceClass_UUID ||
            guid == HandsfreeServiceClass_UUID ||
            guid == HeadsetServiceClass_UUID);
}

// 辅助：判断服务是否为 GATT 服务
bool IsGATTService(const GUID& guid) {
    return (guid.Data1 == 0x1800 || guid.Data1 == 0x1801) &&
           guid.Data2 == 0x0000 && guid.Data3 == 0x1000;
}

// 连接蓝牙设备（参考提供的代码：通过禁用/启用音频相关服务触发连接）
bool ConnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    AddLog(L"尝试连接设备: " + deviceName + L" [" + BluetoothAddressToString(address) + L"]");

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        AddLog(L"  获取设备信息失败: " + to_wstring(result) + L" " + Win32ErrorToString(result));
        return false;
    }

    if (deviceInfo.fConnected) {
        AddLog(L"  设备已连接");
        return true;
    }

    HANDLE hRadio = OpenFirstRadio();
    if (!hRadio) {
        AddLog(L"  未找到蓝牙适配器");
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
            AddLog(L"  成功启用服务: " + GuidToString(svc));
            Sleep(1200);

            // 检查是否已连接
            DWORD r2 = BluetoothGetDeviceInfo(NULL, &deviceInfo);
            if (r2 == ERROR_SUCCESS && deviceInfo.fConnected) {
                AddLog(L"  连接成功");
                CloseHandle(hRadio);
                return true;
            }
        } else if (r == ERROR_SERVICE_DOES_NOT_EXIST) {
            // 跳过未安装的服务
        } else {
            AddLog(L"  启用服务失败: " + to_wstring(r) + L" " + Win32ErrorToString(r));
        }
    }

    // 最终再检查一次连接状态
    DWORD r3 = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (r3 == ERROR_SUCCESS && deviceInfo.fConnected) {
        AddLog(L"  连接成功");
        CloseHandle(hRadio);
        return true;
    }

    CloseHandle(hRadio);
    AddLog(L"  连接失败");
    return false;
}

// 断开蓝牙设备（遍历已安装服务逐一禁用）
bool DisconnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    wstring msg = L"尝试断开设备: " + deviceName + L" [" + BluetoothAddressToString(address) + L"]";
    AddLog(msg);

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        AddLog(L"  获取设备信息失败: " + to_wstring(result) + L" " + Win32ErrorToString(result));
        return false;
    }

    if (!deviceInfo.fConnected) {
        AddLog(L"  设备未连接");
        return true;
    }

    HANDLE hRadio = OpenFirstRadio();
    if (!hRadio) {
        AddLog(L"  无法打开本地蓝牙适配器");
        return false;
    }
    
    vector<GUID> serviceGuids;
    const DWORD CAP = 32;
    GUID guids[CAP] = {};
    DWORD returned = CAP;
    result = BluetoothEnumerateInstalledServices(hRadio, &deviceInfo, &returned, guids);
    if (result == ERROR_SUCCESS && returned > 0) {
        for (DWORD i = 0; i < returned; ++i) serviceGuids.push_back(guids[i]);
    }

    if (serviceGuids.empty()) {
        serviceGuids.push_back(HumanInterfaceDeviceServiceClass_UUID);      // HID
        serviceGuids.push_back(HandsfreeServiceClass_UUID);                 // 免提
        serviceGuids.push_back(AudioSinkServiceClass_UUID);                 // 音频接收器
        serviceGuids.push_back(AudioSourceServiceClass_UUID);               // 音频源
        serviceGuids.push_back(HeadsetServiceClass_UUID);                   // 头戴式/耳机
        serviceGuids.push_back(AVRemoteControlTargetServiceClass_UUID);     // AVRCP 目标
        serviceGuids.push_back(AVRemoteControlServiceClass_UUID);           // AVRCP 控制器
        serviceGuids.push_back(SerialPortServiceClass_UUID);                // 串口
    }

    bool ok = false;
    for (const auto& svc : serviceGuids) {
        result = BluetoothSetServiceState(hRadio, &deviceInfo, &svc, BLUETOOTH_SERVICE_DISABLE);
        if (result == ERROR_SUCCESS) ok = true;
    }

    if (hRadio) CloseHandle(hRadio);
    this_thread::sleep_for(chrono::milliseconds(500));

    AddLog(ok ? L"  断开成功" : L"  断开失败");

    // 若断开成功，标记此设备禁止自动重连，直到用户手动连接为止
    if (ok) {
        wstring key = BluetoothAddressToString(address);
        g_blockAutoReconnect.insert(key);
        AddLog(L"  已设置为手动断开：自动重连已禁用（直到手动连接）");
    }
    return ok;
}

// 更新设备列表显示
void UpdateDeviceList(const vector<BluetoothDeviceInfo>& devices, const set<wstring>& monitorDevices) {
    if (!g_hwndDeviceList) return;
    
    g_currentDevices = devices;
    g_monitorDevices = monitorDevices;
    
    // 保存当前选中的设备名称
    wstring selectedDeviceName;
    int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
    if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
        selectedDeviceName = g_currentDevices[selectedIndex].name;
    }
    
    ListView_DeleteAllItems(g_hwndDeviceList);
    
    int newSelectedIndex = -1;
    for (size_t i = 0; i < devices.size(); i++) {
        const auto& device = devices[i];
        bool shouldMonitor = !monitorDevices.empty() && MatchAnySubstring(device.name, monitorDevices);
        
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)device.name.c_str();
        ListView_InsertItem(g_hwndDeviceList, &lvi);
        
        wstring addr = BluetoothAddressToString(device.address);
        ListView_SetItemText(g_hwndDeviceList, (int)i, 1, (LPWSTR)addr.c_str());
        
        const wchar_t* status = device.connected ? L"已连接" : L"未连接";
        ListView_SetItemText(g_hwndDeviceList, (int)i, 2, (LPWSTR)status);
        
        const wchar_t* monitor = shouldMonitor ? L"是" : L"否";
        ListView_SetItemText(g_hwndDeviceList, (int)i, 3, (LPWSTR)monitor);
        
        // 记录之前选中的设备的新位置
        if (!selectedDeviceName.empty() && device.name == selectedDeviceName) {
            newSelectedIndex = (int)i;
        }
    }
    
    // 恢复选中状态
    if (newSelectedIndex != -1) {
        ListView_SetItemState(g_hwndDeviceList, newSelectedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(g_hwndDeviceList, newSelectedIndex, FALSE);
    }
}

// 显示设备右键菜单
void ShowDeviceContextMenu(HWND hwnd) {
    int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
    if (selectedIndex == -1) return;
    
    if (selectedIndex >= (int)g_currentDevices.size()) return;
    
    const auto& device = g_currentDevices[selectedIndex];
    bool isMonitored = !g_monitorDevices.empty() && g_monitorDevices.count(device.name) > 0;
    
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    
    if (device.connected) {
        AppendMenu(hMenu, MF_STRING, ID_DEVICE_DISCONNECT, L"断开连接");
    } else {
        AppendMenu(hMenu, MF_STRING, ID_DEVICE_CONNECT, L"手动连接");
    }
    
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    if (isMonitored && !g_monitorDevices.empty()) {
        AppendMenu(hMenu, MF_STRING, ID_DEVICE_REMOVE_MONITOR, L"从监控列表移除");
    } else {
        AppendMenu(hMenu, MF_STRING, ID_DEVICE_ADD_MONITOR, L"添加到监控列表");
    }
    
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_DEVICE_COPY_NAME, L"复制设备名称");
    AppendMenu(hMenu, MF_STRING, ID_DEVICE_COPY_MAC, L"复制MAC地址");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_DEVICE_REFRESH, L"刷新设备列表");
    
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// 监控线程
void MonitorThread() {
    AddLog(L"========================================");
    AddLog(L"蓝牙设备自动连接程序已启动");
    AddLog(L"========================================");
    
    // 使用全局配置，如果为空则从文件加载
    if (g_monitorDevices.empty()) {
        g_monitorDevices = LoadConfig(L"config.txt");
    }
    set<wstring> monitorDevices = g_monitorDevices;
    
vector<BluetoothDeviceInfo> pairedDevices = GetPairedDevicesWithInquiry(true);
    
    if (pairedDevices.empty()) {
        AddLog(L"未找到已配对的蓝牙设备");
        AddLog(L"请先在系统设置中配对蓝牙设备");
        return;
    }

    AddLog(L"找到 " + to_wstring(pairedDevices.size()) + L" 个已配对的设备");
    
    vector<BluetoothDeviceInfo> devicesToMonitor;
    for (const auto& device : pairedDevices) {
        // 只监控配置文件中明确指定的设备（子串匹配）
        bool shouldMonitor = !monitorDevices.empty() && MatchAnySubstring(device.name, monitorDevices);
        
        wstring msg = L"  - " + device.name + L" [" + BluetoothAddressToString(device.address) + L"]";
        msg += device.connected ? L" (已连接)" : L" (未连接)";
        if (shouldMonitor) {
            msg += L" [监控中]";
            devicesToMonitor.push_back(device);
        }
        AddLog(msg);
    }
    
    if (devicesToMonitor.empty()) {
        AddLog(L"没有需要监控的设备");
        AddLog(L"请右键点击设备列表中的设备，选择\"添加到监控列表\"");
        UpdateDeviceList(pairedDevices, monitorDevices);
        return;
    }
    
    UpdateDeviceList(pairedDevices, monitorDevices);
    
    AddLog(L"开始监听设备状态...");
    
    vector<bool> lastConnectedState(devicesToMonitor.size(), false);
    for (size_t i = 0; i < devicesToMonitor.size(); i++) {
        lastConnectedState[i] = devicesToMonitor[i].connected;
    }

    int checkCount = 0;
    int scanCount = 0;
    
    while (g_bRunning) {
        checkCount++;
        
        // 每 3 次检查做一次主动扫描
        bool doInquiry = (checkCount % 3) == 0;
        if (doInquiry) {
            scanCount++;
            AddLog(L"[" + to_wstring(checkCount) + L"] 执行主动扫描 #" + to_wstring(scanCount) + L"...");
        }
        
        vector<BluetoothDeviceInfo> currentDevices = GetPairedDevicesWithInquiry(doInquiry);
        UpdateDeviceList(currentDevices, monitorDevices);
        
        for (size_t i = 0; i < devicesToMonitor.size(); i++) {
            if (!g_bRunning) break;
            
            const auto& pairedDevice = devicesToMonitor[i];
            bool currentlyConnected = false;
            bool deviceFound = false;
            
            for (const auto& current : currentDevices) {
                if (memcmp(&current.address, &pairedDevice.address, sizeof(BLUETOOTH_ADDRESS)) == 0) {
                    currentlyConnected = current.connected;
                    deviceFound = true;
                    break;
                }
            }
            
            if (!deviceFound) {
                continue;
            }
            
            if (currentlyConnected && !lastConnectedState[i]) {
                AddLog(L"[" + to_wstring(checkCount) + L"] ✅ 设备已连接: " + pairedDevice.name);
                lastConnectedState[i] = true;
            }
            else if (!currentlyConnected && lastConnectedState[i]) {
                // 二次确认，避免误判
                BLUETOOTH_DEVICE_INFO di = {0};
                di.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
                di.Address = pairedDevice.address;
                DWORD rchk = BluetoothGetDeviceInfo(NULL, &di);
                if (rchk == ERROR_SUCCESS && di.fConnected) {
                    // 仍然连接，跳过
                } else {
                    AddLog(L"[" + to_wstring(checkCount) + L"] ❌ 设备已断开: " + pairedDevice.name);
                    lastConnectedState[i] = false;
                }
            }
            else if (!currentlyConnected && doInquiry) {
                AddLog(L"[" + to_wstring(checkCount) + L"] 🔍 发现设备未连接，尝试连接: " + pairedDevice.name);
                // 自动重连前检查：是否被手动断开阻止，以及是否处于冷却期
                wstring mac = BluetoothAddressToString(pairedDevice.address);
                if (g_blockAutoReconnect.count(mac) > 0) {
                    AddLog(L"  ⏸ 用户手动断开，跳过自动重连: " + pairedDevice.name);
                    continue;
                }
                auto now = chrono::steady_clock::now();
                auto it = g_lastConnectAttempt.find(mac);
                if (it != g_lastConnectAttempt.end()) {
                    auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - it->second).count();
                    if (elapsed < CONNECT_COOLDOWN_MS) {
                        AddLog(L"  ⏱ 冷却中，跳过本次重连: " + pairedDevice.name);
                        continue;
                    }
                }
                g_lastConnectAttempt[mac] = now;
                if (ConnectDevice(pairedDevice.address, pairedDevice.name)) {
                    lastConnectedState[i] = true;
                }
            }
        }
        
        for (int i = 0; i < 10 && g_bRunning; i++) {
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }
    
    AddLog(L"监控已停止");
}

// 创建托盘图标
void CreateTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, WINDOW_TITLE);
    
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// 显示托盘菜单
void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_SHOW, L"显示窗口");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_CONFIG, L"打开配置文件");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");
    
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        // 创建设备列表
        g_hwndDeviceList = CreateWindowEx(
            0, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
            10, 10, 760, 200,
            hwnd, (HMENU)ID_DEVICE_LIST, g_hInst, NULL
        );
        
        // 设置列表视图列
        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        
        lvc.pszText = (LPWSTR)L"设备名称";
        lvc.cx = 200;
        ListView_InsertColumn(g_hwndDeviceList, 0, &lvc);
        
        lvc.pszText = (LPWSTR)L"MAC地址";
        lvc.cx = 180;
        ListView_InsertColumn(g_hwndDeviceList, 1, &lvc);
        
        lvc.pszText = (LPWSTR)L"连接状态";
        lvc.cx = 100;
        ListView_InsertColumn(g_hwndDeviceList, 2, &lvc);
        
        lvc.pszText = (LPWSTR)L"监控";
        lvc.cx = 80;
        ListView_InsertColumn(g_hwndDeviceList, 3, &lvc);
        
        ListView_SetExtendedListViewStyle(g_hwndDeviceList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        
        // 创建日志编辑框
        g_hwndLog = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 250, 760, 280,
            hwnd, (HMENU)ID_LOG_EDIT, g_hInst, NULL
        );
        
        // 创建按钮
        CreateWindow(
            L"BUTTON", L"开始监控",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 540, 100, 30,
            hwnd, (HMENU)ID_BTN_START, g_hInst, NULL
        );
        
        CreateWindow(
            L"BUTTON", L"停止监控",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 540, 100, 30,
            hwnd, (HMENU)ID_BTN_STOP, g_hInst, NULL
        );
        
        CreateWindow(
            L"BUTTON", L"清空日志",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            230, 540, 100, 30,
            hwnd, (HMENU)ID_BTN_CLEAR, g_hInst, NULL
        );
        
        // 创建托盘图标
        CreateTrayIcon(hwnd);
        
        // 初始化配置文件（首次运行创建空配置）
        wifstream testFile(L"config.txt");
        if (!testFile.good()) {
            // 文件不存在，创建空配置
            set<wstring> emptyConfig;
            SaveConfig(L"config.txt", emptyConfig);
        }
        testFile.close();
        
        // 自动开始监控
        g_bRunning = true;
        g_pMonitorThread = new thread(MonitorThread);
        
        break;
    }
    
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_START:
            if (!g_bRunning) {
                g_bRunning = true;
                g_pMonitorThread = new thread(MonitorThread);
                AddLog(L"监控已启动");
            }
            break;
            
        case ID_BTN_STOP:
            if (g_bRunning) {
                g_bRunning = false;
                AddLog(L"正在停止监控...");
                
                // 异步等待线程结束
                if (g_pMonitorThread) {
                    thread cleanup_thread([](thread* pThread) {
                        if (pThread && pThread->joinable()) {
                            pThread->join();
                            delete pThread;
                        }
                    }, g_pMonitorThread);
                    cleanup_thread.detach();
                    g_pMonitorThread = nullptr;
                }
                
                AddLog(L"监控已停止");
            }
            break;
            
        case ID_BTN_CLEAR:
            SetWindowText(g_hwndLog, L"");
            break;
            
        case ID_TRAY_SHOW:
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            break;
            
        case ID_TRAY_CONFIG:
            ShellExecute(NULL, L"open", L"notepad.exe", L"config.txt", NULL, SW_SHOW);
            break;
            
        case ID_TRAY_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
            
        case ID_DEVICE_CONNECT:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                thread([device]() {
                    // 手动连接前，取消自动重连阻止
                    wstring mac = BluetoothAddressToString(device.address);
                    g_blockAutoReconnect.erase(mac);
                    ConnectDevice(device.address, device.name);
                    Sleep(1000);
vector<BluetoothDeviceInfo> devices = GetPairedDevicesWithInquiry(true);
                    UpdateDeviceList(devices, g_monitorDevices);
                }).detach();
            }
            break;
        }
        
        case ID_DEVICE_DISCONNECT:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                thread([device]() {
                    DisconnectDevice(device.address, device.name);
                    Sleep(1000);
vector<BluetoothDeviceInfo> devices = GetPairedDevicesWithInquiry(true);
                    UpdateDeviceList(devices, g_monitorDevices);
                }).detach();
            }
            break;
        }
        
        case ID_DEVICE_COPY_NAME:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    size_t size = (device.name.length() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), device.name.c_str(), size);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();
                    AddLog(L"已复制设备名称: " + device.name);
                }
            }
            break;
        }
        
        case ID_DEVICE_COPY_MAC:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                wstring macAddr = BluetoothAddressToString(device.address);
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    size_t size = (macAddr.length() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), macAddr.c_str(), size);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();
                    AddLog(L"已复制MAC地址: " + macAddr);
                }
            }
            break;
        }
        
        case ID_DEVICE_REFRESH:
        {
            AddLog(L"正在刷新设备列表...");
vector<BluetoothDeviceInfo> devices = GetPairedDevicesWithInquiry(true);
            UpdateDeviceList(devices, g_monitorDevices);
            AddLog(L"设备列表已刷新");
            break;
        }
        
        case ID_DEVICE_ADD_MONITOR:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                
                // 重新加载配置
                g_monitorDevices = LoadConfig(L"config.txt");
                g_monitorDevices.insert(device.name);
                
                if (SaveConfig(L"config.txt", g_monitorDevices)) {
                    AddLog(L"已添加到监控列表: " + device.name);
                    
                    // 重启监控线程以应用更改
                    if (g_bRunning && g_pMonitorThread) {
                        g_bRunning = false;
                        AddLog(L"正在重启监控...");
                        
                        thread* oldThread = g_pMonitorThread;
                        g_pMonitorThread = nullptr;
                        
                        // 异步清理旧线程
                        thread([oldThread, hwnd]() {
                            if (oldThread && oldThread->joinable()) {
                                oldThread->join();
                            }
                            delete oldThread;
                            
                            // 启动新线程
                            Sleep(500);
                            g_bRunning = true;
                            g_pMonitorThread = new thread(MonitorThread);
                        }).detach();
                    } else if (!g_bRunning) {
                        // 如果监控未运行，启动它
                        g_bRunning = true;
                        g_pMonitorThread = new thread(MonitorThread);
                    }
                    
                    // 更新显示
vector<BluetoothDeviceInfo> devices = GetPairedDevicesWithInquiry(true);
                    UpdateDeviceList(devices, g_monitorDevices);
                } else {
                    AddLog(L"添加失败: 无法保存配置文件");
                }
            }
            break;
        }
        
        case ID_DEVICE_REMOVE_MONITOR:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                
                // 重新加载配置
                g_monitorDevices = LoadConfig(L"config.txt");
                g_monitorDevices.erase(device.name);
                
                if (SaveConfig(L"config.txt", g_monitorDevices)) {
                    AddLog(L"已从监控列表移除: " + device.name);
                    
                    // 重启监控线程以应用更改
                    if (g_bRunning && g_pMonitorThread) {
                        g_bRunning = false;
                        AddLog(L"正在重启监控...");
                        
                        thread* oldThread = g_pMonitorThread;
                        g_pMonitorThread = nullptr;
                        
                        // 异步清理旧线程
                        thread([oldThread, hwnd]() {
                            if (oldThread && oldThread->joinable()) {
                                oldThread->join();
                            }
                            delete oldThread;
                            
                            // 启动新线程（如果还有设备需要监控）
                            Sleep(500);
                            if (!g_monitorDevices.empty()) {
                                g_bRunning = true;
                                g_pMonitorThread = new thread(MonitorThread);
                            }
                        }).detach();
                    }
                    
                    // 更新显示
vector<BluetoothDeviceInfo> devices = GetPairedDevicesWithInquiry(true);
                    UpdateDeviceList(devices, g_monitorDevices);
                } else {
                    AddLog(L"移除失败: 无法保存配置文件");
                }
            }
            break;
        }
        }
        break;
    
    case WM_NOTIFY:
    {
        LPNMHDR pnmhdr = (LPNMHDR)lParam;
        if (pnmhdr->idFrom == ID_DEVICE_LIST && pnmhdr->code == NM_RCLICK) {
            ShowDeviceContextMenu(hwnd);
        }
        break;
    }
    
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        break;
    
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
        }
        break;
    
    case WM_CLOSE:
        if (MessageBox(hwnd, L"确定要退出程序吗？", L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            // 停止监控
            g_bRunning = false;
            
            // 删除托盘图标
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            
            // 直接销毁窗口，让WM_DESTROY处理线程清理
            DestroyWindow(hwnd);
        }
        break;
    
    case WM_DESTROY:
        // 确保监控已停止
        g_bRunning = false;
        
        // 异步清理线程，不阻塞UI
        if (g_pMonitorThread) {
            thread* pThreadToClean = g_pMonitorThread;
            g_pMonitorThread = nullptr;
            
            // 创建一个清理线程
            thread([pThreadToClean]() {
                if (pThreadToClean) {
                    if (pThreadToClean->joinable()) {
                        pThreadToClean->join();
                    }
                    delete pThreadToClean;
                }
            }).detach();
        }
        
        PostQuitMessage(0);
        break;
    
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;
    
    // 初始化通用控件
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    // 注册窗口类
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    RegisterClassW(&wc);
    
    // 创建窗口
    g_hwndMain = CreateWindowExW(
        0, CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 620,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwndMain) {
        return 0;
    }
    
    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);
    
    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
