// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef SUCCESS
#undef SUCCESS
#endif

#include <iostream>
#include <mutex>
#include <atomic>

namespace unpaker {

class ApplicationManager {
public:
    static ApplicationManager& getInstance() {
        static ApplicationManager instance;
        return instance;
    }

    ApplicationManager(const ApplicationManager&) = delete;
    ApplicationManager& operator=(const ApplicationManager&) = delete;

    bool acquireInstance(const char* unique_id = "unPAKer_Mutex") {
        m_mutex_handle = CreateMutexA(nullptr, FALSE, unique_id);
        if (!m_mutex_handle) {
            std::cerr << "[ERROR] Failed to create instance mutex" << std::endl;
            return false;
        }

        DWORD wait_result = WaitForSingleObject(m_mutex_handle, 0);

        if (wait_result == WAIT_OBJECT_0) {
            m_is_primary = true;
            std::cout << "[INFO] Application instance acquired successfully" << std::endl;
            return true;
        } else if (wait_result == WAIT_TIMEOUT || wait_result == WAIT_ABANDONED) {
            std::cerr << "[ERROR] Another instance of unPAKer is already running" << std::endl;
            CloseHandle(m_mutex_handle);
            m_mutex_handle = nullptr;
            return false;
        } else {
            std::cerr << "[ERROR] Failed to acquire instance mutex (error: " << GetLastError() << ")" << std::endl;
            CloseHandle(m_mutex_handle);
            m_mutex_handle = nullptr;
            return false;
        }
    }

    void releaseInstance() {
        if (m_mutex_handle && m_is_primary) {
            ReleaseMutex(m_mutex_handle);
            CloseHandle(m_mutex_handle);
            m_mutex_handle = nullptr;
            m_is_primary = false;
            std::cout << "[INFO] Instance released" << std::endl;
        }
    }

    bool isPrimary() const {
        return m_is_primary;
    }

    static void registerShutdownHandler() {
        if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)) {
            std::cerr << "[WARNING] Failed to register console control handler" << std::endl;
        }
    }

    static void requestShutdown() {
        s_shutdown_requested = true;
        std::cout << "[INFO] Shutdown requested" << std::endl;
    }

    static bool isShutdownRequested() {
        return s_shutdown_requested.load();
    }

    ~ApplicationManager() {
        releaseInstance();
    }

private:
    ApplicationManager() : m_mutex_handle(nullptr), m_is_primary(false) {}

    static BOOL WINAPI consoleCtrlHandler(DWORD ctrl_type) {
        switch (ctrl_type) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                std::cout << "[INFO] Received shutdown signal (" << ctrl_type << ")" << std::endl;
                requestShutdown();
                return TRUE;
            default:
                return FALSE;
        }
    }

    HANDLE m_mutex_handle;
    bool m_is_primary;
    static std::atomic<bool> s_shutdown_requested;
};

} // namespace unpaker
