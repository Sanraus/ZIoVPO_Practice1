#pragma once

#include <windows.h>

class SingleInstance
{
public:
    SingleInstance();
    ~SingleInstance();

    bool IsFirstInstance() const;

private:
    HANDLE m_mutex{};
    bool m_isFirstInstance{ false };
};