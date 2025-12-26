// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#include "config.hpp"
#include "logger.hpp"
#include <fstream>
#include <iostream>
#include <shlobj.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#pragma comment(lib, "shell32.lib")

namespace unpaker {

Config& Config::instance() {
    static Config _instance;
    return _instance;
}

Config::Config()
    : current_theme(ThemeType::SYSTEM),
              last_file_format(0),
              dev_mode(false) {
    load_from_disk();
}

fs::path Config::get_config_path() {
    wchar_t temp_path[MAX_PATH] = {0};
    if (GetTempPathW(MAX_PATH, temp_path)) {
        fs::path temp_dir(temp_path);
        fs::path config_dir = temp_dir / L"unPAKer";

        try {
            if (!fs::exists(config_dir)) {
                fs::create_directories(config_dir);
            }
            return config_dir / L"config.ini";
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[WARNING] Failed to create config directory: " << e.what() << std::endl;
        }
    }

    return fs::path();
}

void Config::set_theme(ThemeType theme) {
    current_theme = theme;
    save_to_disk();
}

ThemeType Config::get_theme() const {
    return current_theme;
}

COLORREF Config::get_bg_color() const {
    switch (current_theme) {
        case ThemeType::OLD_LIGHT: return RGB(240, 240, 240);
        case ThemeType::OLD_DARK: return RGB(30, 30, 30);
        case ThemeType::NEW_LIGHT: return RGB(248, 248, 248);
        case ThemeType::NEW_DARK: return RGB(25, 25, 25);
        case ThemeType::SYSTEM:
        default:
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD value = 1;
                DWORD size = sizeof(value);
                LONG ret = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);
                RegCloseKey(hKey);

                if (ret == ERROR_SUCCESS && value == 0) {
                    return RGB(30, 30, 30);
                }
            }
            return RGB(240, 240, 240);
    }
}

COLORREF Config::get_text_color() const {
    switch (current_theme) {
        case ThemeType::OLD_LIGHT: return RGB(0, 0, 0);
        case ThemeType::OLD_DARK: return RGB(220, 220, 220);
        case ThemeType::NEW_LIGHT: return RGB(0, 0, 0);
        case ThemeType::NEW_DARK: return RGB(230, 230, 230);
        case ThemeType::SYSTEM:
        default:
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD value = 1;
                DWORD size = sizeof(value);
                LONG ret = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&value, &size);
                RegCloseKey(hKey);

                if (ret == ERROR_SUCCESS && value == 0) {
                    return RGB(220, 220, 220);
                }
            }
            return RGB(0, 0, 0);
    }
}

void Config::set_last_file_format(uint32_t format_index) {
    last_file_format = format_index;
    save_to_disk();
}

uint32_t Config::get_last_file_format() const {
    return last_file_format;
}

void Config::set_dev_mode(bool enabled) {
    dev_mode = enabled;
    save_to_disk();
}

bool Config::get_dev_mode() const {
    return dev_mode;
}

void Config::load_from_disk() {
    fs::path config_file = get_config_path();

    if (config_file.empty() || !fs::exists(config_file)) {
        current_theme = ThemeType::SYSTEM;
        last_file_format = 0;
        dev_mode = false;
        return;
    }

    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "[WARNING] Failed to open config file for reading" << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.find("theme=") == 0) {
                int theme_val = std::stoi(line.substr(6));
                if (theme_val >= 0 && theme_val <= 4) {
                    current_theme = static_cast<ThemeType>(theme_val);
                }
            } else if (line.find("file_format=") == 0) {
                last_file_format = std::stoul(line.substr(12));
            } else if (line.find("dev_mode=") == 0) {
                std::string value = line.substr(9);
                dev_mode = (value == "1" || value == "true" || value == "True");
            }
        }

        file.close();
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Error loading config: " << e.what() << std::endl;
    }
}

void Config::save_to_disk() {
    fs::path config_file = get_config_path();

    if (config_file.empty()) {
        std::cerr << "[WARNING] Config path is empty, cannot save" << std::endl;
        return;
    }

    try {
        std::ofstream file(config_file, std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[WARNING] Failed to open config file for writing" << std::endl;
            return;
        }

        file << "theme=" << static_cast<int>(current_theme) << "\n";
        file << "file_format=" << last_file_format << "\n";
        file << "dev_mode=" << (dev_mode ? "1" : "0") << "\n";

        file.close();
        DEBUG_COUT("[DEBUG] Config saved to: " << config_file.string() << std::endl);
    } catch (const std::exception& e) {
        std::cerr << "[WARNING] Error saving config: " << e.what() << std::endl;
    }
}

} // namespace unpaker
