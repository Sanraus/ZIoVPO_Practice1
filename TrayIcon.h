#pragma once

#include <windows.h>
#include <shellapi.h>

class TrayIcon
{
public:
    explicit TrayIcon(HWND hwnd);
    ~TrayIcon();

    bool Initialize();
    void Remove();

    void ShowMainWindow();
    void HideMainWindow();
    void ShowContextMenu();
    void ExitApplication();

private:
    bool AddIcon();

    static LRESULT CALLBACK SubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR refData
    );

private:
    HWND m_hwnd{};
    NOTIFYICONDATAW m_nid{};
    UINT m_taskbarCreatedMessage{};
    bool m_iconAdded{ false };
};