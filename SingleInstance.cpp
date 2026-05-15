#include "pch.h"
#include "SingleInstance.h"

SingleInstance::SingleInstance()
{
    m_mutex = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\Practice1_SingleInstance_Mutex"
    );

    if (m_mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        m_isFirstInstance = true;
    }
}

SingleInstance::~SingleInstance()
{
    if (m_mutex != nullptr)
    {
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }
}

bool SingleInstance::IsFirstInstance() const
{
    return m_isFirstInstance;
}