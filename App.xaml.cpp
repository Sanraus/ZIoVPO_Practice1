#include "pch.h"
#include "SingleInstance.h"
#include <shellapi.h>
#include <string>
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

bool HasHiddenArgument(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e)
{
    std::wstring launchArguments = e.Arguments().c_str();

    if (launchArguments.find(L"--hidden") != std::wstring::npos)
    {
        return true;
    }

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

namespace winrt::Practice1::implementation
{
    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
            {
                if (IsDebuggerPresent())
                {
                    auto errorMessage = e.Message();
                    __debugbreak();
                }
            });
#endif
    }

    void App::OnLaunched(LaunchActivatedEventArgs const& e)
    {
        static bool alreadyLaunched = false;

        // Если это повторная активация уже запущенного WinUI-приложения,
        // не создаём второе окно и вторую иконку.
        if (alreadyLaunched)
        {
            return;
        }

        static SingleInstance singleInstance;

        // Если это реально второй процесс, сразу завершаем его.
        // Важно: это происходит ДО создания MainWindow,
        // значит вторая иконка в трей не добавится.
        if (!singleInstance.IsFirstInstance())
        {
            ExitProcess(0);
        }

        alreadyLaunched = true;

        window = make<MainWindow>();

        if (!HasHiddenArgument(e))
        {
            window.Activate();
        }
    }
}