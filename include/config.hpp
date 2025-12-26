// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include <string>
#include <cstdint>
#include <filesystem>

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

namespace fs = std::filesystem;

namespace unpaker {

enum class ThemeType {
    OLD_LIGHT = 0,
    OLD_DARK = 1,
    NEW_LIGHT = 2,
    NEW_DARK = 3,
    SYSTEM = 4
};

class Config {
public:
    static Config& instance();

    void set_theme(ThemeType theme);
    ThemeType get_theme() const;

    COLORREF get_bg_color() const;
    COLORREF get_text_color() const;

    void set_last_file_format(uint32_t format_index);
    uint32_t get_last_file_format() const;

    void set_dev_mode(bool enabled);
    bool get_dev_mode() const;

    void load_from_disk();
    void save_to_disk();

private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    ThemeType current_theme;
    uint32_t last_file_format;
    bool dev_mode;
    fs::path config_path;

    fs::path get_config_path();
};

} // namespace unpaker
