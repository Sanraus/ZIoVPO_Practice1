#include "pch.h"
#include "MainWindow.xaml.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.ui.xaml.window.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::Practice1::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        auto windowNative = this->try_as<::IWindowNative>();
        winrt::check_bool(windowNative);

        winrt::check_hresult(windowNative->get_WindowHandle(&m_hwnd));

        m_trayIcon = std::make_unique<TrayIcon>(m_hwnd);
        m_trayIcon->Initialize();
    }

    void MainWindow::ExitMenuItem_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&
    )
    {
        if (m_trayIcon)
        {
            m_trayIcon->ExitApplication();
        }
        else
        {
            Application::Current().Exit();
        }
    }
}