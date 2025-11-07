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

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
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
    wofstream file(configFile);
    
    if (!file.is_open()) {
        return false;
    }
    
    file << L"# 蓝牙设备自动连接配置文件\r\n";
    file << L"# 每行填写一个要监控的设备名称\r\n";
    file << L"# 使用 # 开头的行为注释\r\n";
    file << L"# 如果此文件为空或不存在，将监控所有已配对的设备\r\n";
    file << L"\r\n";
    
    for (const auto& deviceName : monitorDevices) {
        file << deviceName << L"\r\n";
    }
    
    file.close();
    return true;
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

// 连接蓝牙设备
bool ConnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    wstring msg = L"尝试连接设备: " + deviceName + L" [" + BluetoothAddressToString(address) + L"]";
    AddLog(msg);

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        AddLog(L"  获取设备信息失败");
        return false;
    }

    if (deviceInfo.fConnected) {
        AddLog(L"  设备已连接");
        return true;
    }

    result = BluetoothSetServiceState(NULL, &deviceInfo, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);
    
    if (result == ERROR_SUCCESS) {
        this_thread::sleep_for(chrono::milliseconds(500));
        result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
        if (result == ERROR_SUCCESS && deviceInfo.fConnected) {
            AddLog(L"  连接成功");
            return true;
        }
    }
    
    AddLog(L"  连接失败");
    return false;
}

// 断开蓝牙设备
bool DisconnectDevice(const BLUETOOTH_ADDRESS& address, const wstring& deviceName) {
    wstring msg = L"尝试断开设备: " + deviceName + L" [" + BluetoothAddressToString(address) + L"]";
    AddLog(msg);

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);
    deviceInfo.Address = address;

    DWORD result = BluetoothGetDeviceInfo(NULL, &deviceInfo);
    if (result != ERROR_SUCCESS) {
        AddLog(L"  获取设备信息失败");
        return false;
    }

    if (!deviceInfo.fConnected) {
        AddLog(L"  设备未连接");
        return true;
    }

    result = BluetoothSetServiceState(NULL, &deviceInfo, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_DISABLE);
    
    if (result == ERROR_SUCCESS) {
        this_thread::sleep_for(chrono::milliseconds(500));
        AddLog(L"  断开成功");
        return true;
    }
    
    AddLog(L"  断开失败");
    return false;
}

// 更新设备列表显示
void UpdateDeviceList(const vector<BluetoothDeviceInfo>& devices, const set<wstring>& monitorDevices) {
    if (!g_hwndDeviceList) return;
    
    g_currentDevices = devices;
    g_monitorDevices = monitorDevices;
    
    ListView_DeleteAllItems(g_hwndDeviceList);
    
    for (size_t i = 0; i < devices.size(); i++) {
        const auto& device = devices[i];
        bool shouldMonitor = monitorDevices.empty() || monitorDevices.count(device.name) > 0;
        
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
    }
}

// 显示设备右键菜单
void ShowDeviceContextMenu(HWND hwnd) {
    int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
    if (selectedIndex == -1) return;
    
    if (selectedIndex >= (int)g_currentDevices.size()) return;
    
    const auto& device = g_currentDevices[selectedIndex];
    bool isMonitored = g_monitorDevices.empty() || g_monitorDevices.count(device.name) > 0;
    
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
    
    set<wstring> monitorDevices = LoadConfig(L"config.txt");
    
    vector<BluetoothDeviceInfo> pairedDevices = GetPairedDevices();
    
    if (pairedDevices.empty()) {
        AddLog(L"未找到已配对的蓝牙设备");
        AddLog(L"请先在系统设置中配对蓝牙设备");
        return;
    }

    AddLog(L"找到 " + to_wstring(pairedDevices.size()) + L" 个已配对的设备");
    
    vector<BluetoothDeviceInfo> devicesToMonitor;
    for (const auto& device : pairedDevices) {
        bool shouldMonitor = monitorDevices.empty() || monitorDevices.count(device.name) > 0;
        
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
        AddLog(L"请在 config.txt 中配置设备名称");
        return;
    }
    
    UpdateDeviceList(pairedDevices, monitorDevices);
    
    AddLog(L"开始监听设备状态...");
    
    vector<bool> lastConnectedState(devicesToMonitor.size(), false);
    for (size_t i = 0; i < devicesToMonitor.size(); i++) {
        lastConnectedState[i] = devicesToMonitor[i].connected;
    }

    int checkCount = 0;
    while (g_bRunning) {
        checkCount++;
        
        vector<BluetoothDeviceInfo> currentDevices = GetPairedDevices();
        UpdateDeviceList(currentDevices, monitorDevices);
        
        for (size_t i = 0; i < devicesToMonitor.size(); i++) {
            if (!g_bRunning) break;
            
            const auto& pairedDevice = devicesToMonitor[i];
            bool currentlyConnected = false;
            
            for (const auto& current : currentDevices) {
                if (memcmp(&current.address, &pairedDevice.address, sizeof(BLUETOOTH_ADDRESS)) == 0) {
                    currentlyConnected = current.connected;
                    break;
                }
            }
            
            if (currentlyConnected && !lastConnectedState[i]) {
                wstring msg = L"[" + to_wstring(checkCount) + L"] 检测到设备上线: " + pairedDevice.name;
                AddLog(msg);
                lastConnectedState[i] = true;
            }
            else if (!currentlyConnected && lastConnectedState[i]) {
                wstring msg = L"[" + to_wstring(checkCount) + L"] 检测到设备离线: " + pairedDevice.name;
                AddLog(msg);
                lastConnectedState[i] = false;
            }
            else if (!currentlyConnected) {
                wstring msg = L"[" + to_wstring(checkCount) + L"] 设备未连接，尝试建立连接: " + pairedDevice.name;
                AddLog(msg);
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
                    ConnectDevice(device.address, device.name);
                    Sleep(1000);
                    vector<BluetoothDeviceInfo> devices = GetPairedDevices();
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
                    vector<BluetoothDeviceInfo> devices = GetPairedDevices();
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
            vector<BluetoothDeviceInfo> devices = GetPairedDevices();
            UpdateDeviceList(devices, g_monitorDevices);
            AddLog(L"设备列表已刷新");
            break;
        }
        
        case ID_DEVICE_ADD_MONITOR:
        {
            int selectedIndex = ListView_GetNextItem(g_hwndDeviceList, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < (int)g_currentDevices.size()) {
                const auto& device = g_currentDevices[selectedIndex];
                
                if (g_monitorDevices.empty()) {
                    g_monitorDevices = LoadConfig(L"config.txt");
                    if (g_monitorDevices.empty()) {
                        for (const auto& dev : g_currentDevices) {
                            g_monitorDevices.insert(dev.name);
                        }
                    }
                }
                
                g_monitorDevices.insert(device.name);
                
                if (SaveConfig(L"config.txt", g_monitorDevices)) {
                    AddLog(L"已添加到监控列表: " + device.name);
                    vector<BluetoothDeviceInfo> devices = GetPairedDevices();
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
                
                g_monitorDevices.erase(device.name);
                
                if (SaveConfig(L"config.txt", g_monitorDevices)) {
                    AddLog(L"已从监控列表移除: " + device.name);
                    vector<BluetoothDeviceInfo> devices = GetPairedDevices();
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
