// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include "pak_parser.hpp"
#include <string>
#include <memory>

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

namespace unpaker {

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    bool initialize(int width, int height, const std::string& title);
    void shutdown();
    void update();
    void render();
    bool should_close() const;
    void load_archive(const fs::path& pak_path);

private:
    HWND main_window;
    HWND tree_view;
    HWND info_text;
    HWND file_path_text;
    HWND open_button;
    HWND status_bar;

    HFONT hFont;

    int window_width;
    int window_height;
    std::string window_title;
    bool is_running;
    bool is_loading;

    std::shared_ptr<PakParser> parser;
    std::string loaded_archive_path;

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

    void create_controls();
    void populate_tree_view();
    void display_archive_info();
    void preview_file(const std::shared_ptr<FileEntry>& file);
    void handle_open_file();
    void set_status_text(const std::string& text);
    std::shared_ptr<FileEntry> find_file_in_tree(const std::shared_ptr<DirectoryEntry>& dir, const std::string& filename);
    void check_for_updates();

    friend LRESULT CALLBACK gui_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

} // namespace unpaker
