#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <rpc.h>
#include <tlhelp32.h>

#include <cstdlib>

#include "Practice2Rpc.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Advapi32.lib")

namespace
{
    constexpr const wchar_t* WINDOW_CLASS_NAME = L"Practice2GuiWindowClass";
    constexpr const wchar_t* WINDOW_TITLE = L"Practice2Gui";
    constexpr const wchar_t* RPC_ENDPOINT = L"Practice2RpcEndpoint";

    constexpr UINT WM_TRAY_ICON = WM_APP + 100;

    constexpr UINT ID_TRAY_OPEN = 1001;
    constexpr UINT ID_TRAY_EXIT = 1002;
    constexpr UINT ID_MENU_EXIT = 2001;

    constexpr UINT TRAY_ICON_ID = 1;

    HINSTANCE g_instance = nullptr;
    HWND g_window = nullptr;
    NOTIFYICONDATAW g_trayIcon{};
    bool g_trayIconAdded = false;

    bool WaitForServiceRunning(SC_HANDLE serviceHandle)
    {
        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded = 0;

        for (int i = 0; i < 30; ++i)
        {
            BOOL queried = QueryServiceStatusEx(
                serviceHandle,
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &bytesNeeded
            );

            if (!queried)
            {
                return false;
            }

            if (status.dwCurrentState == SERVICE_RUNNING)
            {
                return true;
            }

            Sleep(500);
        }

        return false;
    }

    bool EnsureServiceRunningOrExit()
    {
        SC_HANDLE serviceManager = OpenSCManagerW(
            nullptr,
            nullptr,
            SC_MANAGER_CONNECT
        );

        if (serviceManager == nullptr)
        {
            return false;
        }

        SC_HANDLE serviceHandle = OpenServiceW(
            serviceManager,
            L"Practice2Service",
            SERVICE_QUERY_STATUS
        );

        if (serviceHandle == nullptr)
        {
            CloseServiceHandle(serviceManager);
            return false;
        }

        SERVICE_STATUS_PROCESS status{};
        DWORD bytesNeeded = 0;

        BOOL queried = QueryServiceStatusEx(
            serviceHandle,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status),
            sizeof(status),
            &bytesNeeded
        );

        CloseServiceHandle(serviceHandle);

        if (!queried)
        {
            CloseServiceHandle(serviceManager);
            return false;
        }

        if (status.dwCurrentState == SERVICE_RUNNING)
        {
            CloseServiceHandle(serviceManager);
            return true;
        }

        if (status.dwCurrentState == SERVICE_STOPPED)
        {
            SC_HANDLE startHandle = OpenServiceW(
                serviceManager,
                L"Practice2Service",
                SERVICE_QUERY_STATUS | SERVICE_START
            );

            if (startHandle == nullptr)
            {
                CloseServiceHandle(serviceManager);
                return false;
            }

            StartServiceW(startHandle, 0, nullptr);
            WaitForServiceRunning(startHandle);

            CloseServiceHandle(startHandle);
            CloseServiceHandle(serviceManager);

            /*
                При ручном запуске GUI только запускает службу и завершает
                текущий экземпляр. Новый GUI запускается уже службой.
            */
            return false;
        }

        CloseServiceHandle(serviceManager);
        return false;
    }

    DWORD GetParentProcessId()
    {
        DWORD currentProcessId = GetCurrentProcessId();
        DWORD parentProcessId = 0;

        HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        PROCESSENTRY32W processEntry{};
        processEntry.dwSize = sizeof(processEntry);

        if (Process32FirstW(snapshot, &processEntry))
        {
            do
            {
                if (processEntry.th32ProcessID == currentProcessId)
                {
                    parentProcessId = processEntry.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }

        CloseHandle(snapshot);

        return parentProcessId;
    }

    bool IsParentProcessService()
    {
        DWORD parentProcessId = GetParentProcessId();

        if (parentProcessId == 0)
        {
            return false;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        bool parentIsService = false;

        PROCESSENTRY32W processEntry{};
        processEntry.dwSize = sizeof(processEntry);

        if (Process32FirstW(snapshot, &processEntry))
        {
            do
            {
                if (processEntry.th32ProcessID == parentProcessId)
                {
                    parentIsService =
                        _wcsicmp(processEntry.szExeFile, L"Practice2.exe") == 0;

                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }

        CloseHandle(snapshot);

        return parentIsService;
    }

    bool HasHiddenArgument()
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        if (argv == nullptr)
        {
            return false;
        }

        bool hidden = false;

        for (int i = 1; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"--hidden") == 0)
            {
                hidden = true;
                break;
            }
        }

        LocalFree(argv);

        return hidden;
    }

    bool StopServiceByRpc()
    {
        RPC_WSTR stringBinding = nullptr;

        RPC_STATUS status = RpcStringBindingComposeW(
            nullptr,
            reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(L"ncalrpc")),
            nullptr,
            reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(RPC_ENDPOINT)),
            nullptr,
            &stringBinding
        );

        if (status != RPC_S_OK)
        {
            return false;
        }

        status = RpcBindingFromStringBindingW(
            stringBinding,
            &Practice2RpcBinding
        );

        RpcStringFreeW(&stringBinding);

        if (status != RPC_S_OK)
        {
            return false;
        }

        bool stopped = false;

        RpcTryExcept
        {
            StopPractice2Service();
            stopped = true;
        }
            RpcExcept(1)
        {
            stopped = false;
        }
        RpcEndExcept

            if (Practice2RpcBinding)
            {
                RpcBindingFree(&Practice2RpcBinding);
                Practice2RpcBinding = nullptr;
            }

        return stopped;
    }

    bool AddTrayIcon(HWND window)
    {
        ZeroMemory(&g_trayIcon, sizeof(g_trayIcon));

        g_trayIcon.cbSize = sizeof(g_trayIcon);
        g_trayIcon.hWnd = window;
        g_trayIcon.uID = TRAY_ICON_ID;
        g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_trayIcon.uCallbackMessage = WM_TRAY_ICON;
        g_trayIcon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

        StringCchCopyW(
            g_trayIcon.szTip,
            ARRAYSIZE(g_trayIcon.szTip),
            L"Practice2Gui"
        );

        BOOL added = Shell_NotifyIconW(NIM_ADD, &g_trayIcon);

        if (added)
        {
            g_trayIcon.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &g_trayIcon);
            g_trayIconAdded = true;
        }

        return added == TRUE;
    }

    void RemoveTrayIcon()
    {
        if (g_trayIconAdded)
        {
            Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
            g_trayIconAdded = false;
        }
    }

    void ShowMainWindow()
    {
        ShowWindow(g_window, SW_SHOW);
        ShowWindow(g_window, SW_RESTORE);
        SetForegroundWindow(g_window);
    }

    void HideMainWindow()
    {
        ShowWindow(g_window, SW_HIDE);
    }

    void ExitApplication()
    {
        /*
            Выход из GUI выполняется через RPC-команду остановки службы.
            Служба после остановки завершает все запущенные GUI-процессы.
        */
        if (StopServiceByRpc())
        {
            return;
        }

        /*
            Локальное закрытие используется только если RPC-сервер недоступен.
        */
        RemoveTrayIcon();
        DestroyWindow(g_window);
        PostQuitMessage(0);
    }

    void ShowTrayMenu()
    {
        POINT cursorPosition{};
        GetCursorPos(&cursorPosition);

        HMENU menu = CreatePopupMenu();

        AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"\u041e\u0442\u043a\u0440\u044b\u0442\u044c");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"\u0412\u044b\u0445\u043e\u0434");

        SetForegroundWindow(g_window);

        UINT command = TrackPopupMenu(
            menu,
            TPM_RIGHTBUTTON | TPM_RETURNCMD,
            cursorPosition.x,
            cursorPosition.y,
            0,
            g_window,
            nullptr
        );

        DestroyMenu(menu);

        if (command == ID_TRAY_OPEN)
        {
            ShowMainWindow();
        }
        else if (command == ID_TRAY_EXIT)
        {
            ExitApplication();
        }
    }

    HMENU CreateMainMenu()
    {
        HMENU mainMenu = CreateMenu();
        HMENU fileMenu = CreatePopupMenu();

        AppendMenuW(fileMenu, MF_STRING, ID_MENU_EXIT, L"\u0412\u044b\u0445\u043e\u0434");
        AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"\u0424\u0430\u0439\u043b");

        return mainMenu;
    }

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        static UINT taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

        if (message == taskbarCreatedMessage)
        {
            AddTrayIcon(window);
            return 0;
        }

        switch (message)
        {
        case WM_CREATE:
            AddTrayIcon(window);
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_MENU_EXIT)
            {
                ExitApplication();
                return 0;
            }
            break;

        case WM_CLOSE:
            HideMainWindow();
            return 0;

        case WM_TRAY_ICON:
        {
            UINT eventMessage = LOWORD(lParam);

            if (eventMessage == WM_LBUTTONUP || eventMessage == WM_LBUTTONDBLCLK)
            {
                ShowMainWindow();
                return 0;
            }

            if (eventMessage == WM_RBUTTONUP || eventMessage == WM_CONTEXTMENU)
            {
                ShowTrayMenu();
                return 0;
            }

            break;
        }

        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool RegisterMainWindowClass()
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = g_instance;
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = WINDOW_CLASS_NAME;

        return RegisterClassExW(&windowClass) != 0;
    }

    HWND CreateMainWindow()
    {
        return CreateWindowExW(
            0,
            WINDOW_CLASS_NAME,
            WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            600,
            350,
            nullptr,
            CreateMainMenu(),
            g_instance,
            nullptr
        );
    }
}

extern "C" void* __RPC_USER midl_user_allocate(size_t size)
{
    return std::malloc(size);
}

extern "C" void __RPC_USER midl_user_free(void* pointer)
{
    std::free(pointer);
}

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int
)
{
    g_instance = instance;
    if (!EnsureServiceRunningOrExit())
    {
        return 0;
    }

    if (!IsParentProcessService())
    {
        return 0;
    }

    HANDLE mutex = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\Practice2Gui_SingleInstance_Mutex"
    );

    if (mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 0;
    }

    if (!RegisterMainWindowClass())
    {
        return 1;
    }

    g_window = CreateMainWindow();

    if (g_window == nullptr)
    {
        return 1;
    }

    if (!HasHiddenArgument())
    {
        ShowMainWindow();
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (mutex)
    {
        CloseHandle(mutex);
    }

    return static_cast<int>(message.wParam);
}