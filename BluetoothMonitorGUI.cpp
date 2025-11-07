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

// 全局变量
HINSTANCE g_hInst = nullptr;
HWND g_hwndMain = nullptr;
HWND g_hwndLog = nullptr;
HWND g_hwndDeviceList = nullptr;
NOTIFYICONDATAW g_nid = {};
bool g_bRunning = true;
mutex g_logMutex;
thread* g_pMonitorThread = nullptr;

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

// 蓝牙设备信息结构
struct BluetoothDeviceInfo {
    BLUETOOTH_ADDRESS address;
    wstring name;
    bool connected;
};

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

// 更新设备列表显示
void UpdateDeviceList(const vector<BluetoothDeviceInfo>& devices, const set<wstring>& monitorDevices) {
    if (!g_hwndDeviceList) return;
    
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
                if (g_pMonitorThread && g_pMonitorThread->joinable()) {
                    g_pMonitorThread->join();
                    delete g_pMonitorThread;
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
        }
        break;
    
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
            g_bRunning = false;
            if (g_pMonitorThread && g_pMonitorThread->joinable()) {
                g_pMonitorThread->join();
                delete g_pMonitorThread;
            }
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            DestroyWindow(hwnd);
        }
        break;
    
    case WM_DESTROY:
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
