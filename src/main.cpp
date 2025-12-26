// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
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
    setvbuf(stdout, nullptr, _IOLBF, 1024);

    // Enable virtual terminal processing for modern console color support
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode);
        }
    }

    #ifdef _DEBUG
        bool dev_mode = true;
    #else
        bool dev_mode = false;
    #endif

    unpaker::Logger::instance().initialize(dev_mode);

    unpaker::Logger::instance().info("========================================");
    unpaker::Logger::instance().info(std::string("unPAKer v") + UNPAKER_VERSION);
    unpaker::Logger::instance().info("Game Resource Archive Extractor");
    unpaker::Logger::instance().info(std::string("Author: ") + UNPAKER_AUTHOR);
    unpaker::Logger::instance().info(std::string("License: ") + UNPAKER_LICENSE);
    unpaker::Logger::instance().info("========================================");

    auto& app_manager = unpaker::ApplicationManager::getInstance();
    if (!app_manager.acquireInstance("unPAKer_SingleInstance")) {
        unpaker::Logger::instance().error("Failed to acquire application instance");
        return 1;
    }

    unpaker::ApplicationManager::registerShutdownHandler();

    unpaker::GuiManager gui;

    if (!gui.initialize(1200, 700, std::string("unPAKer v" UNPAKER_VERSION))) {
        unpaker::Logger::instance().error("Failed to initialize GUI");
        return 1;
    }

    if (argc > 1) {
        std::string pak_path = argv[1];
        unpaker::Logger::instance().info(std::string("Loading archive: ") + pak_path);
        gui.load_archive(pak_path);
    }

    unpaker::Logger::instance().info("GUI initialized. Window is running...");
    unpaker::Logger::instance().info("Close window to exit.");
    unpaker::Logger::instance().info("========================================");

    while (!gui.should_close()) {
        gui.update();
        gui.render();
    }

    gui.shutdown();

    unpaker::Logger::instance().success("unPAKer closed successfully.");

    unpaker::unpaker_memory_checkpoint("Final");

    unpaker::Logger::instance().shutdown();

    return 0;
}
