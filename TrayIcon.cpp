#include "pch.h"
#include "TrayIcon.h"

#include <commctrl.h>
#include <strsafe.h>
#include <winrt/Microsoft.UI.Xaml.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

namespace
{
    constexpr UINT WM_TRAY_ICON = WM_APP + 100;

    constexpr UINT ID_TRAY_OPEN = 1001;
    constexpr UINT ID_TRAY_EXIT = 1002;

    constexpr UINT TRAY_ICON_ID = 1;
}

TrayIcon::TrayIcon(HWND hwnd)
    : m_hwnd(hwnd)
{
    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
}

TrayIcon::~TrayIcon()
{
    Remove();
    RemoveWindowSubclass(m_hwnd, TrayIcon::SubclassProc, 1);
}

bool TrayIcon::Initialize()
{
    SetWindowSubclass(
        m_hwnd,
        TrayIcon::SubclassProc,
        1,
        reinterpret_cast<DWORD_PTR>(this)
    );

    return AddIcon();
}

bool TrayIcon::AddIcon()
{
    ZeroMemory(&m_nid, sizeof(m_nid));

    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = TRAY_ICON_ID;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_ICON;
    m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    StringCchCopyW(
        m_nid.szTip,
        ARRAYSIZE(m_nid.szTip),
        L"Practice1"
    );

    BOOL added = Shell_NotifyIconW(NIM_ADD, &m_nid);

    if (added)
    {
        m_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
        m_iconAdded = true;
    }

    return added == TRUE;
}

void TrayIcon::Remove()
{
    if (m_iconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_iconAdded = false;
    }
}

void TrayIcon::ShowMainWindow()
{
    ShowWindow(m_hwnd, SW_SHOW);
    ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
}

void TrayIcon::HideMainWindow()
{
    ShowWindow(m_hwnd, SW_HIDE);
}

void TrayIcon::ShowContextMenu()
{
    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);

    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"\u041e\u0442\u043a\u0440\u044b\u0442\u044c");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"\u0412\u044b\u0445\u043e\u0434");

    SetForegroundWindow(m_hwnd);

    UINT command = TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD,
        cursorPosition.x,
        cursorPosition.y,
        0,
        m_hwnd,
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

void TrayIcon::ExitApplication()
{
    Remove();

    auto app = winrt::Microsoft::UI::Xaml::Application::Current();

    if (app)
    {
        app.Exit();
    }
}

LRESULT CALLBACK TrayIcon::SubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR,
    DWORD_PTR refData
)
{
    auto self = reinterpret_cast<TrayIcon*>(refData);

    if (!self)
    {
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    if (message == self->m_taskbarCreatedMessage)
    {
        self->AddIcon();
        return 0;
    }

    if (message == WM_CLOSE)
    {
        self->HideMainWindow();
        return 0;
    }

    if (message == WM_TRAY_ICON)
    {
        UINT eventMessage = LOWORD(lParam);

        if (eventMessage == WM_LBUTTONUP || eventMessage == WM_LBUTTONDBLCLK)
        {
            self->ShowMainWindow();
            return 0;
        }

        if (eventMessage == WM_RBUTTONUP || eventMessage == WM_CONTEXTMENU)
        {
            self->ShowContextMenu();
            return 0;
        }
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}