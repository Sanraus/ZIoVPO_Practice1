#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <rpc.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "Practice2Rpc.h"

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Rpcrt4.lib")

namespace
{
    constexpr const wchar_t* SERVICE_NAME = L"Practice2Service";
    constexpr const wchar_t* RPC_ENDPOINT = L"Practice2RpcEndpoint";

    SERVICE_STATUS_HANDLE g_serviceStatusHandle = nullptr;
    SERVICE_STATUS g_serviceStatus{};
    HANDLE g_stopEvent = nullptr;

    struct StartedProcess
    {
        DWORD sessionId{};
        HANDLE processHandle{};
        HANDLE threadHandle{};
    };

    std::vector<StartedProcess> g_startedProcesses;

    void SetServiceStatusState(
        DWORD currentState,
        DWORD win32ExitCode = NO_ERROR,
        DWORD waitHint = 0
    )
    {
        g_serviceStatus.dwCurrentState = currentState;
        g_serviceStatus.dwWin32ExitCode = win32ExitCode;
        g_serviceStatus.dwWaitHint = waitHint;

        if (currentState == SERVICE_START_PENDING)
        {
            g_serviceStatus.dwControlsAccepted = 0;
        }
        else if (currentState == SERVICE_RUNNING)
        {
            /*
                Stop и Shutdown отключены по требованиям задания.

                SERVICE_ACCEPT_STOP и SERVICE_ACCEPT_SHUTDOWN
                не указываются специально.

                SERVICE_ACCEPT_SESSIONCHANGE нужен для получения
                событий входа пользователей в терминальные сессии.
            */
            g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SESSIONCHANGE;
        }
        else
        {
            g_serviceStatus.dwControlsAccepted = 0;
        }

        SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
    }

    std::wstring GetServiceDirectory()
    {
        wchar_t path[MAX_PATH]{};

        GetModuleFileNameW(nullptr, path, MAX_PATH);

        std::wstring fullPath = path;
        size_t slashPosition = fullPath.find_last_of(L"\\/");

        if (slashPosition == std::wstring::npos)
        {
            return L".";
        }

        return fullPath.substr(0, slashPosition);
    }

    bool FileExists(const std::wstring& path)
    {
        DWORD attributes = GetFileAttributesW(path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring FindGuiApplicationPath()
    {
        /*
            Поиск графического приложения рядом с исполняемым файлом службы.

            Для Debug-сборки возможный путь:
            C:\ZIoVPO\Practice1\Practice1\x64\Debug\Practice2Gui.exe
        */

        std::wstring serviceDir = GetServiceDirectory();

        std::vector<std::wstring> candidates =
        {
            serviceDir + L"\\Practice2Gui.exe",
            serviceDir + L"\\Practice2Gui\\Practice2Gui.exe"
        };

        for (const auto& candidate : candidates)
        {
            if (FileExists(candidate))
            {
                return candidate;
            }
        }

        return L"";
    }

    bool IsProcessAlreadyStartedForSession(DWORD sessionId)
    {
        for (auto& process : g_startedProcesses)
        {
            if (process.sessionId == sessionId)
            {
                DWORD exitCode = 0;

                if (GetExitCodeProcess(process.processHandle, &exitCode) &&
                    exitCode == STILL_ACTIVE)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void RemoveFinishedProcesses()
    {
        auto newEnd = std::remove_if(
            g_startedProcesses.begin(),
            g_startedProcesses.end(),
            [](StartedProcess& process)
            {
                DWORD exitCode = 0;

                bool finished =
                    !GetExitCodeProcess(process.processHandle, &exitCode) ||
                    exitCode != STILL_ACTIVE;

                if (finished)
                {
                    if (process.threadHandle)
                    {
                        CloseHandle(process.threadHandle);
                    }

                    if (process.processHandle)
                    {
                        CloseHandle(process.processHandle);
                    }
                }

                return finished;
            }
        );

        g_startedProcesses.erase(newEnd, g_startedProcesses.end());
    }

    bool LaunchGuiInSession(DWORD sessionId)
    {
        if (sessionId == 0)
        {
            return false;
        }

        RemoveFinishedProcesses();

        if (IsProcessAlreadyStartedForSession(sessionId))
        {
            return true;
        }

        std::wstring guiPath = FindGuiApplicationPath();

        if (guiPath.empty())
        {
            return false;
        }

        HANDLE userToken = nullptr;

        if (!WTSQueryUserToken(sessionId, &userToken))
        {
            return false;
        }

        HANDLE primaryToken = nullptr;

        BOOL duplicated = DuplicateTokenEx(
            userToken,
            TOKEN_ASSIGN_PRIMARY |
            TOKEN_DUPLICATE |
            TOKEN_IMPERSONATE |
            TOKEN_QUERY |
            TOKEN_ADJUST_DEFAULT |
            TOKEN_ADJUST_SESSIONID,
            nullptr,
            SecurityIdentification,
            TokenPrimary,
            &primaryToken
        );

        CloseHandle(userToken);

        if (!duplicated)
        {
            return false;
        }

        void* environment = nullptr;

        if (!CreateEnvironmentBlock(&environment, primaryToken, FALSE))
        {
            environment = nullptr;
        }

        std::wstring commandLine = L"\"" + guiPath + L"\" --hidden";

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");

        PROCESS_INFORMATION processInfo{};

        BOOL created = CreateProcessAsUserW(
            primaryToken,
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_UNICODE_ENVIRONMENT,
            environment,
            nullptr,
            &startupInfo,
            &processInfo
        );

        if (environment)
        {
            DestroyEnvironmentBlock(environment);
        }

        CloseHandle(primaryToken);

        if (!created)
        {
            return false;
        }

        StartedProcess startedProcess{};
        startedProcess.sessionId = sessionId;
        startedProcess.processHandle = processInfo.hProcess;
        startedProcess.threadHandle = processInfo.hThread;

        g_startedProcesses.push_back(startedProcess);

        return true;
    }

    void LaunchGuiInExistingUserSessions()
    {
        PWTS_SESSION_INFOW sessions = nullptr;
        DWORD sessionCount = 0;

        if (!WTSEnumerateSessionsW(
            WTS_CURRENT_SERVER_HANDLE,
            0,
            1,
            &sessions,
            &sessionCount
        ))
        {
            return;
        }

        for (DWORD i = 0; i < sessionCount; ++i)
        {
            DWORD sessionId = sessions[i].SessionId;
            WTS_CONNECTSTATE_CLASS state = sessions[i].State;

            if (sessionId == 0)
            {
                continue;
            }

            /*
                WTSActive — пользователь вошёл в систему, сессия активна.
                WTSConnected — подключённая терминальная сессия.

                Сессия 0 исключается, так как в ней работает служба.
            */
            if (state == WTSActive || state == WTSConnected)
            {
                LaunchGuiInSession(sessionId);
            }
        }

        WTSFreeMemory(sessions);
    }

    void TerminateStartedGuiProcesses()
    {
        for (auto& process : g_startedProcesses)
        {
            if (process.processHandle)
            {
                DWORD exitCode = 0;

                if (GetExitCodeProcess(process.processHandle, &exitCode) &&
                    exitCode == STILL_ACTIVE)
                {
                    TerminateProcess(process.processHandle, 0);
                }
            }

            if (process.threadHandle)
            {
                CloseHandle(process.threadHandle);
                process.threadHandle = nullptr;
            }

            if (process.processHandle)
            {
                CloseHandle(process.processHandle);
                process.processHandle = nullptr;
            }
        }

        g_startedProcesses.clear();
    }

    bool StartRpcServer()
    {
        RPC_STATUS status = RpcServerUseProtseqEpW(
            reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(L"ncalrpc")),
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(RPC_ENDPOINT)),
            nullptr
        );

        if (status != RPC_S_OK)
        {
            return false;
        }

        status = RpcServerRegisterIf2(
            Practice2Rpc_v1_0_s_ifspec,
            nullptr,
            nullptr,
            RPC_IF_ALLOW_LOCAL_ONLY,
            RPC_C_LISTEN_MAX_CALLS_DEFAULT,
            static_cast<unsigned int>(-1),
            nullptr
        );

        if (status != RPC_S_OK)
        {
            return false;
        }

        status = RpcServerListen(
            1,
            RPC_C_LISTEN_MAX_CALLS_DEFAULT,
            FALSE
        );

        return status == RPC_S_OK || status == RPC_S_ALREADY_LISTENING;
    }

    void StopRpcServer()
    {
        RpcMgmtStopServerListening(nullptr);
        RpcServerUnregisterIf(
            Practice2Rpc_v1_0_s_ifspec,
            nullptr,
            FALSE
        );
    }

    DWORD WINAPI ServiceControlHandlerEx(
        DWORD control,
        DWORD eventType,
        LPVOID eventData,
        LPVOID context
    )
    {
        UNREFERENCED_PARAMETER(context);

        if (control == SERVICE_CONTROL_SESSIONCHANGE)
        {
            if (eventType == WTS_SESSION_LOGON ||
                eventType == WTS_SESSION_UNLOCK ||
                eventType == WTS_CONSOLE_CONNECT ||
                eventType == WTS_REMOTE_CONNECT)
            {
                auto notification =
                    reinterpret_cast<WTSSESSION_NOTIFICATION*>(eventData);

                if (notification)
                {
                    LaunchGuiInSession(notification->dwSessionId);
                }
            }

            return NO_ERROR;
        }

        /*
            Stop и Shutdown не обрабатываются.
            Остановка службы выполняется через RPC-интерфейс.
        */

        return NO_ERROR;
    }

    void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
    {
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);

        g_serviceStatusHandle = RegisterServiceCtrlHandlerExW(
            SERVICE_NAME,
            ServiceControlHandlerEx,
            nullptr
        );

        if (g_serviceStatusHandle == nullptr)
        {
            return;
        }

        ZeroMemory(&g_serviceStatus, sizeof(g_serviceStatus));

        g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        g_serviceStatus.dwServiceSpecificExitCode = 0;

        SetServiceStatusState(SERVICE_START_PENDING, NO_ERROR, 3000);

        g_stopEvent = CreateEventW(
            nullptr,
            TRUE,
            FALSE,
            nullptr
        );

        if (g_stopEvent == nullptr)
        {
            SetServiceStatusState(SERVICE_STOPPED, GetLastError());
            return;
        }

        SetServiceStatusState(SERVICE_RUNNING);

        /*
            Запуск графического приложения во всех пользовательских
            терминальных сессиях, кроме сессии 0.
        */
        LaunchGuiInExistingUserSessions();

        /*
            RPC-сервер использует локальный ALPC-транспорт ncalrpc.
            Служба остаётся в состоянии Running до остановки RPC-сервера.
        */
        StartRpcServer();

        SetServiceStatusState(SERVICE_STOP_PENDING, NO_ERROR, 3000);

        /*
            При остановке службы завершаются все графические приложения,
            запущенные этой службой.
        */
        TerminateStartedGuiProcesses();

        StopRpcServer();

        if (g_stopEvent)
        {
            CloseHandle(g_stopEvent);
            g_stopEvent = nullptr;
        }

        SetServiceStatusState(SERVICE_STOPPED);
    }
}

extern "C" void StopPractice2Service(void)
{
    /*
        RPC-команда остановки службы.
        Обычные Stop и Shutdown через SCM не используются.
    */
    if (g_stopEvent)
    {
        SetEvent(g_stopEvent);
    }

    RpcMgmtStopServerListening(nullptr);
}

extern "C" void* __RPC_USER midl_user_allocate(size_t size)
{
    return std::malloc(size);
}

extern "C" void __RPC_USER midl_user_free(void* pointer)
{
    std::free(pointer);
}

int wmain()
{
    SERVICE_TABLE_ENTRYW serviceTable[] =
    {
        { const_cast<LPWSTR>(SERVICE_NAME), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherW(serviceTable))
    {
        return static_cast<int>(GetLastError());
    }

    return 0;
}