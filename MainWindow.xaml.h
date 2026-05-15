#pragma once

#include "MainWindow.g.h"
#include "TrayIcon.h"

#include <memory>
#include <windows.h>

namespace winrt::Practice1::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void ExitMenuItem_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args
        );

    private:
        HWND m_hwnd{};
        std::unique_ptr<TrayIcon> m_trayIcon;
    };
}

namespace winrt::Practice1::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {};
}