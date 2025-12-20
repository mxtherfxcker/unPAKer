// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#define UNPAKER_MEMORY_TRACKING
#include "memory_tracker.hpp"
#include "application_manager.hpp"
#include "pak_parser.hpp"
#include "gui_manager.hpp"
#include "version.hpp"
#include "logger.hpp"
#include "config.hpp"
#include <iostream>
#include <string>
#include <clocale>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int main(int argc, char* argv[]) {
    typedef BOOL (WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        auto SetProcessDpiAwarenessContextFunc_ptr = (SetProcessDpiAwarenessContextFunc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (SetProcessDpiAwarenessContextFunc_ptr) {
            SetProcessDpiAwarenessContextFunc_ptr(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        }
        FreeLibrary(user32);
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1024);

    #ifdef _DEBUG
        bool dev_mode = true;
    #else
        bool dev_mode = false;
    #endif

    unpaker::Logger::instance().initialize(dev_mode);

    std::cout << "========================================" << std::endl;
    std::cout << "unPAKer v" << UNPAKER_VERSION << std::endl;
    std::cout << "Game Resource Archive Extractor" << std::endl;
    std::cout << "Author: " << UNPAKER_AUTHOR << std::endl;
    std::cout << "License: " << UNPAKER_LICENSE << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    auto& app_manager = unpaker::ApplicationManager::getInstance();
    if (!app_manager.acquireInstance("unPAKer_SingleInstance")) {
        std::cerr << "[FATAL] Failed to acquire application instance" << std::endl;
        return 1;
    }

    unpaker::ApplicationManager::registerShutdownHandler();

    unpaker::GuiManager gui;

    if (!gui.initialize(1200, 700, std::string("unPAKer v" UNPAKER_VERSION))) {
        std::cerr << "[ERROR] Failed to initialize GUI" << std::endl;
        return 1;
    }

    if (argc > 1) {
        std::string pak_path = argv[1];
        std::cout << "[INFO] Loading archive: " << pak_path << std::endl;
        gui.load_archive(pak_path);
    }

    std::cout << "[INFO] GUI initialized. Window is running..." << std::endl;
    std::cout << "[INFO] Close window to exit." << std::endl;
    std::cout << "========================================" << std::endl;

    while (!gui.should_close()) {
        gui.update();
        gui.render();
    }

    gui.shutdown();

    std::cout << "[SUCCESS] unPAKer closed successfully." << std::endl;

    unpaker::unpaker_memory_checkpoint("Final");

    unpaker::Logger::instance().shutdown();

    return 0;
}
