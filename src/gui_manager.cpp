// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#define UNICODE
#define _UNICODE

#include "gui_manager.hpp"
#include "version.hpp"
#include "file_validator.hpp"
#include "config.hpp"
#include "logger.hpp"

using unpaker::Logger;
#include <commctrl.h>
#include <shlwapi.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <algorithm>
#include <cctype>
#include <regex>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winhttp.lib")

namespace unpaker {

static GuiManager* g_gui_instance = nullptr;

LRESULT CALLBACK GuiManager::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lparam);
        GuiManager* pThis = reinterpret_cast<GuiManager*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return 0;
    }

    GuiManager* pThis = nullptr;
    if (msg != WM_CREATE) {
        pThis = reinterpret_cast<GuiManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handle_message(msg, wparam, lparam);
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

GuiManager::GuiManager()
    : main_window(nullptr),
              tree_view(nullptr),
              info_text(nullptr),
              file_path_text(nullptr),
              open_button(nullptr),
              status_bar(nullptr),
              hFont(nullptr),
              window_width(0),
              window_height(0),
              window_title(""),
              is_running(true),
              is_loading(false),
              parser(nullptr) {
    g_gui_instance = this;
}

GuiManager::~GuiManager() {
    shutdown();
}

bool GuiManager::initialize(int width, int height, const std::string& title) {
    window_width = width;
    window_height = height;
    window_title = title;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS;

    if (!InitCommonControlsEx(&icex)) {
        std::cerr << "[ERROR] Failed to initialize common controls" << std::endl;
        return false;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hIcon = LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"unPAKerWindowClass";
    wc.hIconSm = LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(101));

    if (!RegisterClassExW(&wc)) {
        std::cerr << "[ERROR] Failed to register window class" << std::endl;
        return false;
    }

    std::wstring wide_title(title.begin(), title.end());

    main_window = CreateWindowExW(
        WS_EX_LEFT,
        L"unPAKerWindowClass",
        wide_title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_width, window_height,
        nullptr, nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!main_window) {
        std::cerr << "[ERROR] Failed to create main window" << std::endl;
        return false;
    }

    SetWindowTextW(main_window, wide_title.c_str());

    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    create_controls();

    ShowWindow(main_window, SW_SHOW);
    UpdateWindow(main_window);

    SetWindowPos(main_window, nullptr, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    RedrawWindow(main_window, nullptr, nullptr,
                                 RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);

    SetWindowTextW(main_window, wide_title.c_str());

    Logger::instance().info("GUI initialized successfully");
    return true;
}

void GuiManager::create_controls() {
    if (!main_window) return;

    HMENU menu_bar = CreateMenu();
    HMENU file_menu = CreateMenu();

    AppendMenu(file_menu, MF_STRING, 1001, L"Open Archive\tCtrl+O");
    AppendMenu(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(file_menu, MF_STRING, 1002, L"Exit\tCtrl+Q");
    AppendMenu(menu_bar, MF_POPUP, (UINT_PTR)file_menu, L"File");

    HMENU help_menu = CreateMenu();
    AppendMenu(help_menu, MF_STRING, 1003, L"About");
    AppendMenu(help_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(help_menu, MF_STRING, 1005, L"Check for updates");
    AppendMenu(menu_bar, MF_POPUP, (UINT_PTR)help_menu, L"Help");

    SetMenu(main_window, menu_bar);

    if (!window_title.empty()) {
        std::wstring wide_title(window_title.begin(), window_title.end());
        SetWindowTextW(main_window, wide_title.c_str());
    }

    int toolbar_height = 40;
    HWND label = CreateWindowEx(
        0, L"STATIC", L"Archive path:",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        10, 10, 85, 20,
        main_window, nullptr, GetModuleHandle(nullptr), nullptr
    );

    file_path_text = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_READONLY,
        100, 10, 400, 20,
        main_window, nullptr, GetModuleHandle(nullptr), nullptr
    );

    open_button = CreateWindowEx(
        0, L"BUTTON", L"Open...",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        510, 10, 80, 20,
        main_window, (HMENU)1004, GetModuleHandle(nullptr), nullptr
    );

    tree_view = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
        WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | WS_CLIPCHILDREN,
        10, toolbar_height + 10, 400, window_height - toolbar_height - 60,
        main_window, nullptr, GetModuleHandle(nullptr), nullptr
    );

    info_text = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        420, toolbar_height + 10, window_width - 430, window_height - toolbar_height - 60,
        main_window, nullptr, GetModuleHandle(nullptr), nullptr
    );

    status_bar = CreateWindowEx(
        0, STATUSCLASSNAME, L"Ready",
        WS_VISIBLE | WS_CHILD,
        0, window_height - 20, window_width, 20,
        main_window, nullptr, GetModuleHandle(nullptr), nullptr
    );

    hFont = CreateFontW(
        18,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,// nCharSet
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, // nClipPrecision
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    SendMessage(tree_view, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(info_text, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(file_path_text, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(label, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(open_button, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(status_bar, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(file_path_text, WM_SETFONT, (WPARAM)hFont, TRUE);

    InvalidateRect(label, nullptr, FALSE);
    InvalidateRect(file_path_text, nullptr, FALSE);
    InvalidateRect(open_button, nullptr, FALSE);
    InvalidateRect(tree_view, nullptr, FALSE);
    InvalidateRect(info_text, nullptr, FALSE);
    InvalidateRect(status_bar, nullptr, FALSE);

}

LRESULT GuiManager::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_COMMAND: {
            int cmd_id = LOWORD(wparam);
            if (cmd_id == 1001) { // Open Archive
                handle_open_file();
            } else if (cmd_id == 1002) { // Exit
                PostQuitMessage(0);
            } else if (cmd_id == 1003) { // About
                std::string version_str = UNPAKER_VERSION;
                std::string date_str = UNPAKER_BUILD_DATE;
                
                int version_len = MultiByteToWideChar(CP_UTF8, 0, version_str.c_str(), -1, NULL, 0);
                int date_len = MultiByteToWideChar(CP_UTF8, 0, date_str.c_str(), -1, NULL, 0);
                
                std::vector<wchar_t> version_wide(version_len);
                std::vector<wchar_t> date_wide(date_len);
                
                MultiByteToWideChar(CP_UTF8, 0, version_str.c_str(), -1, version_wide.data(), version_len);
                MultiByteToWideChar(CP_UTF8, 0, date_str.c_str(), -1, date_wide.data(), date_len);
                
                std::wstring about_text = L"unPAKer v";
                about_text += version_wide.data();
                about_text += L"\n\nGame Resource Archive Extractor\n\nAuthor: mxtherfxcker\n\nLicense: MIT\n\nBuilt on ";
                about_text += date_wide.data();
                MessageBoxW(main_window, about_text.c_str(), L"About unPAKer", MB_OK | MB_ICONINFORMATION);
                version_str.clear();
                version_str.shrink_to_fit();
                date_str.clear();
                date_str.shrink_to_fit();
                version_wide.clear();
                version_wide.shrink_to_fit();
                date_wide.clear();
                date_wide.shrink_to_fit();
                about_text.clear();
                about_text.shrink_to_fit();
            } else if (cmd_id == 1004) { // Open button
                handle_open_file();
            } else if (cmd_id == 1005) { // Check for updates
                check_for_updates();
            }
            return 0;
        }

        case WM_NOTIFY: {
            LPNMHDR hdr = reinterpret_cast<LPNMHDR>(lparam);
            if (hdr->hwndFrom == tree_view && hdr->code == TVN_SELCHANGED) {
                LPNMTREEVIEW pnmtv = reinterpret_cast<LPNMTREEVIEW>(lparam);
                TVITEMW item = pnmtv->itemNew;

                if (item.hItem != NULL) {
                    wchar_t item_text[MAX_PATH] = {0};
                    TVITEMW tvitem = {};
                    tvitem.hItem = item.hItem;
                    tvitem.mask = TVIF_TEXT;
                    tvitem.pszText = item_text;
                    tvitem.cchTextMax = MAX_PATH;
                    TreeView_GetItem(tree_view, &tvitem);

                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, item_text, -1, NULL, 0, NULL, NULL);
                    std::string selected_name(size_needed, 0);
                    WideCharToMultiByte(CP_UTF8, 0, item_text, -1, &selected_name[0], size_needed, NULL, NULL);
                    selected_name.pop_back();

                    auto root = parser->get_root();
                    if (root) {
                        std::shared_ptr<FileEntry> found_file = find_file_in_tree(root, selected_name);
                        if (found_file) {
                            preview_file(found_file);
                        }
                    }
                    selected_name.clear();
                    selected_name.shrink_to_fit();
                }
            } else if (hdr->hwndFrom == tree_view && hdr->code == TVN_ITEMEXPANDED) {
                InvalidateRect(tree_view, nullptr, FALSE);
                RedrawWindow(tree_view, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            }
            return 0;
        }

        case WM_CLOSE: {
            is_running = false;
            DestroyWindow(main_window);
            return 0;
        }

        case WM_DESTROY: {
            is_running = false;
            PostQuitMessage(0);
            return 0;
        }

        case WM_SIZE: {
            int new_width = LOWORD(lparam);
            int new_height = HIWORD(lparam);

            if (tree_view) {
                MoveWindow(tree_view, 10, 50, 380, new_height - 70, TRUE);
                InvalidateRect(tree_view, nullptr, FALSE);
            }
            if (info_text) {
                MoveWindow(info_text, 400, 50, new_width - 410, new_height - 70, TRUE);
                InvalidateRect(info_text, nullptr, FALSE);
            }
            if (status_bar) {
                MoveWindow(status_bar, 0, new_height - 20, new_width, 20, TRUE);
                InvalidateRect(status_bar, nullptr, FALSE);
            }

            window_width = new_width;
            window_height = new_height;
            RedrawWindow(main_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return 0;
        }

        default:
            return DefWindowProc(main_window, msg, wparam, lparam);
    }
}

void GuiManager::handle_open_file() {
    if (is_loading) {
        MessageBoxW(main_window, L"Already loading archive!", L"Information", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = main_window;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename) / sizeof(wchar_t);
    ofn.lpstrFilter = L"PAK Files (*.pak)\0*.pak\0VPK Files (*.vpk)\0*.vpk\0All Files (*.*)\0*.*\0";

    uint32_t last_format = Config::instance().get_last_file_format();
    ofn.nFilterIndex = (last_format > 0 && last_format <= 3) ? last_format : 1;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        Config::instance().set_last_file_format(ofn.nFilterIndex);

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &filename[0], (int)wcslen(filename), NULL, 0, NULL, NULL);
        std::string path(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &filename[0], (int)wcslen(filename), &path[0], size_needed, NULL, NULL);
        load_archive(path);
        path.clear();
        path.shrink_to_fit();
    }
}

void GuiManager::load_archive(const fs::path& pak_path) {
    if (is_loading) {
        std::cerr << "[WARNING] Already loading archive" << std::endl;
        return;
    }

    if (!fs::exists(pak_path)) {
        std::cerr << "[ERROR] File does not exist: " << pak_path.string() << std::endl;
        MessageBoxW(main_window, L"File does not exist!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    is_loading = true;
    set_status_text("Loading archive...");

    try {
        parser = std::make_shared<PakParser>(pak_path);
        if (!parser || !parser->parse()) {
            std::string error_msg = "[ERROR] Failed to parse archive: " + pak_path.string();
            std::cerr << error_msg << std::endl;
            MessageBoxW(main_window, L"Failed to parse archive!", L"Error", MB_OK | MB_ICONERROR);
            parser = nullptr;
            is_loading = false;
            error_msg.clear();
            error_msg.shrink_to_fit();
            set_status_text("Ready");
            return;
        }

        loaded_archive_path = pak_path.string();

        std::string path_str = pak_path.string();
        int len = MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, NULL, 0);
        std::vector<wchar_t> wide_path(len);
        MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, wide_path.data(), len);
        SetWindowTextW(file_path_text, wide_path.data());

        populate_tree_view();
        display_archive_info();

        auto root = parser->get_root();
        fs::path archive_path(loaded_archive_path);
        uint64_t archive_size = fs::file_size(archive_path);
        auto validation = FileValidator::validateArchive(root, archive_size);

        if (!validation.is_valid) {
            std::cerr << "[WARNING] Archive validation failed with " << validation.invalid_entries
                                              << " invalid entries" << std::endl;
        }

        Logger::instance().success(std::string("Archive loaded successfully: ") + pak_path.string());
        Logger::instance().info(std::string("Format: ") + parser->get_format_info());
        Logger::instance().info(std::string("Files: ") + std::to_string(parser->get_file_count()));

        char status_buffer[256];
        snprintf(status_buffer, sizeof(status_buffer), "Archive loaded: %u files", parser->get_file_count());
        set_status_text(status_buffer);
        wide_path.clear();
        wide_path.shrink_to_fit();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception while loading archive: " << e.what() << std::endl;
        MessageBoxA(main_window, e.what(), "Error", MB_OK | MB_ICONERROR);
        parser = nullptr;
        set_status_text("Error loading archive");
    }

    is_loading = false;
}

void GuiManager::populate_tree_view() {
    if (!tree_view || !parser) return;

    SendMessage(tree_view, WM_SETREDRAW, FALSE, 0);

    TreeView_DeleteAllItems(tree_view);

    auto root = parser->get_root();
    if (!root) {
        SendMessage(tree_view, WM_SETREDRAW, TRUE, 0);
        return;
    }

    try {
        TVINSERTSTRUCT tvinsert = {};
        tvinsert.hParent = TVI_ROOT;
        tvinsert.hInsertAfter = TVI_LAST;
        tvinsert.item.mask = TVIF_TEXT;

        static std::vector<wchar_t> wide_buffer(MAX_PATH * 2);

        const char* root_name = root->name.empty() ? "Archive" : root->name.c_str();
        int root_len = MultiByteToWideChar(CP_UTF8, 0, root_name, -1, NULL, 0);
        if (root_len > (int)wide_buffer.size()) {
            wide_buffer.resize(root_len * 2);
        }
        MultiByteToWideChar(CP_UTF8, 0, root_name, -1, wide_buffer.data(), root_len);

        tvinsert.item.pszText = wide_buffer.data();
        HTREEITEM root_item = TreeView_InsertItem(tree_view, &tvinsert);

        DEBUG_COUT("[DEBUG] TreeView: Created root item: " << root_name << std::endl);
        DEBUG_COUT("[DEBUG] TreeView: Subdirectories count: " << root->subdirectories.size() << std::endl);

        std::function<void(const std::shared_ptr<DirectoryEntry>&, HTREEITEM)> add_items =
            [&](const std::shared_ptr<DirectoryEntry>& dir, HTREEITEM parent_item) {
                if (!dir) return;

                for (const auto& file : dir->files) {
                    if (!file) continue;

                    const char* file_name = file->name.c_str();
                    int file_len = MultiByteToWideChar(CP_UTF8, 0, file_name, -1, NULL, 0);
                    if (file_len > (int)wide_buffer.size()) {
                        wide_buffer.resize(file_len * 2);
                    }
                    MultiByteToWideChar(CP_UTF8, 0, file_name, -1, wide_buffer.data(), file_len);

                    tvinsert.hParent = parent_item;
                    tvinsert.item.pszText = wide_buffer.data();
                    TreeView_InsertItem(tree_view, &tvinsert);
                }

                for (const auto& subdir : dir->subdirectories) {
                    if (!subdir) continue;

                    const char* dir_name = subdir->name.c_str();
                    int dir_len = MultiByteToWideChar(CP_UTF8, 0, dir_name, -1, NULL, 0);
                    if (dir_len > (int)wide_buffer.size()) {
                        wide_buffer.resize(dir_len * 2);
                    }
                    MultiByteToWideChar(CP_UTF8, 0, dir_name, -1, wide_buffer.data(), dir_len);

                    tvinsert.hParent = parent_item;
                    tvinsert.item.pszText = wide_buffer.data();
                    HTREEITEM new_item = TreeView_InsertItem(tree_view, &tvinsert);

                    add_items(subdir, new_item);
                }
            };

        add_items(root, root_item);

        TreeView_Expand(tree_view, root_item, TVE_EXPAND);

        DEBUG_COUT("[DEBUG] TreeView: Populated successfully" << std::endl);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in populate_tree_view: " << e.what() << std::endl;
    }

    SendMessage(tree_view, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(tree_view, nullptr, TRUE);
    RedrawWindow(tree_view, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void GuiManager::display_archive_info() {
    if (!info_text) return;
    SetWindowTextW(info_text, L"");
}

void GuiManager::preview_file(const std::shared_ptr<FileEntry>& file) {
    if (!file || !info_text || !parser) return;

    size_t dot_pos = file->name.find_last_of('.');
    if (dot_pos == std::string::npos) {
        SetWindowTextW(info_text, L"[File has no extension]");
        return;
    }

    const char* ext_ptr = file->name.c_str() + dot_pos + 1;
    
    static constexpr const char* supported_exts[] = {"txt", "cfg", "ini", "md", "log", "conf", "config", "properties", "xml", "json"};
    bool is_supported = false;
    
    for (const char* supported : supported_exts) {
        const char* ext_check = ext_ptr;
        const char* sup_check = supported;
        while (*ext_check && *sup_check) {
            char ext_lower = static_cast<char>(std::tolower(static_cast<unsigned char>(*ext_check)));
            if (ext_lower != *sup_check) break;
            ++ext_check;
            ++sup_check;
        }
        if (*ext_check == '\0' && *sup_check == '\0') {
            is_supported = true;
            break;
        }
    }

    if (!is_supported) {
        static wchar_t msg_buffer[512];
        int len = swprintf_s(msg_buffer, sizeof(msg_buffer) / sizeof(wchar_t), L"[Unsupported format: .%hs]\n Supported: .txt, .cfg, .ini, .md, .log, .conf, .config, .properties, .xml, .json", ext_ptr);
        if (len > 0) {
            msg_buffer[len] = L'\0';
            SetWindowTextW(info_text, msg_buffer);
        } else {
            SetWindowTextW(info_text, L"[Unsupported file format]");
        }
        return;
    }

    const size_t MAX_PREVIEW_SIZE = 1024 * 1024; // 1MB
    if (file->size > MAX_PREVIEW_SIZE) {
        static wchar_t size_msg[256];
        int len = swprintf_s(size_msg, sizeof(size_msg) / sizeof(wchar_t), L"[File too large for preview: %u MB (max 1 MB)]", static_cast<unsigned int>(file->size / 1024 / 1024));
        if (len > 0) {
            size_msg[len] = L'\0';
            SetWindowTextW(info_text, size_msg);
        } else {
            SetWindowTextW(info_text, L"[File too large for preview]");
        }
        return;
    }

    try {
        std::vector<uint8_t> file_data;
        if (!parser->extract_file(file, file_data)) {
            SetWindowTextW(info_text, L"[Failed to extract file from archive]");
            return;
        }

        std::string content(file_data.begin(), file_data.end());
        file_data.clear();
        file_data.shrink_to_fit();

        DEBUG_COUT("[DEBUG] File content first 20 bytes (hex): ");
        #ifdef _DEBUG
        size_t debug_size = (content.size() < 20) ? content.size() : 20;
        for (size_t i = 0; i < debug_size; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                                              << (unsigned char)content[i] << " " << std::dec;
        }
        std::cout << std::endl;
        #endif

        bool all_null = true;
        for (size_t i = 0; i < content.size() && i < 100; i++) {
            if (content[i] != '\0') {
                all_null = false;
                break;
            }
        }
        if (all_null) {
            std::cerr << "[ERROR] File content appears to be all null bytes!" << std::endl;
            SetWindowTextW(info_text, L"[Error: File content is empty or corrupted]");
            return;
        }

        std::vector<wchar_t> wide_content;
        bool conversion_success = false;

        int len = MultiByteToWideChar(CP_UTF8, 0, content.data(), static_cast<int>(content.size()), NULL, 0);
        if (len > 0) {
            wide_content.resize(len + 1, 0);
            int converted = MultiByteToWideChar(CP_UTF8, 0, content.data(), static_cast<int>(content.size()), wide_content.data(), len);
            if (converted > 0 && converted <= len) {
                wide_content[converted] = L'\0';
                bool has_data = false;
                for (int i = 0; i < converted && i < 100; i++) {
                    if (wide_content[i] != L'\0') {
                        has_data = true;
                        break;
                    }
                }
                if (has_data) {
                    conversion_success = true;
                    DEBUG_COUT("[DEBUG] Converted as UTF-8: " << converted << " wide chars" << std::endl);
                } else {
                    std::cerr << "[WARNING] UTF-8 conversion produced only null bytes" << std::endl;
                }
            } else {
                DWORD error = GetLastError();
                std::cerr << "[WARNING] UTF-8 conversion failed, converted=" << converted << ", error=" << error << std::endl;
            }
        }

        if (!conversion_success) {
            int len_default = MultiByteToWideChar(CP_ACP, 0, content.data(), static_cast<int>(content.size()), NULL, 0);
            if (len_default > 0) {
                wide_content.resize(len_default + 1, 0);
                int converted = MultiByteToWideChar(CP_ACP, 0, content.data(), static_cast<int>(content.size()), wide_content.data(), len_default);
                if (converted > 0 && converted <= len_default) {
                    wide_content[converted] = L'\0';
                    bool has_data = false;
                    for (int i = 0; i < converted && i < 100; i++) {
                        if (wide_content[i] != L'\0') {
                            has_data = true;
                            break;
                        }
                    }
                    if (has_data) {
                        conversion_success = true;
                        DEBUG_COUT("[DEBUG] Converted as system default code page: " << converted << " wide chars" << std::endl);
                    } else {
                        std::cerr << "[WARNING] System code page conversion produced only null bytes" << std::endl;
                    }
                }
            }
        }

        if (conversion_success && !wide_content.empty()) {

            SendMessage(info_text, EM_SETREADONLY, FALSE, 0);

            BOOL result = SetWindowTextW(info_text, wide_content.data());

            size_t verify_size = (wide_content.size() < 1000) ? wide_content.size() : 1000;
            std::vector<wchar_t> verify_text(verify_size + 1, 0);
            GetWindowTextW(info_text, verify_text.data(), static_cast<int>(verify_size));
            bool text_was_set = (wcslen(verify_text.data()) > 0 || wide_content.size() <= 1);

            SendMessage(info_text, EM_SETREADONLY, TRUE, 0);

            SendMessage(info_text, EM_SETSEL, 0, 0);
            SendMessage(info_text, EM_SCROLLCARET, 0, 0);

            InvalidateRect(info_text, nullptr, TRUE);
            RedrawWindow(info_text, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
            UpdateWindow(info_text);
            UpdateWindow(main_window);

            if (result && text_was_set) {
                DEBUG_COUT("[DEBUG] Previewed file: " << file->path << " (" << file->size << " bytes, "
                                                  << (wide_content.size() - 1) << " wide chars)" << std::endl);
            } else {
                DWORD error = GetLastError();
                std::cerr << "[ERROR] SetWindowTextW failed or text not set, result=" << result
                                                  << ", text_was_set=" << text_was_set << ", error: " << error << std::endl;
                DEBUG_CERR("[DEBUG] First 50 chars of content: ");
                #ifdef _DEBUG
                size_t max_chars = (wide_content.size() > 1) ? ((wide_content.size() - 1 < 50) ? (wide_content.size() - 1) : 50) : 0;
                for (size_t i = 0; i < max_chars; i++) {
                    if (wide_content[i] >= 32 && wide_content[i] <= 126) {
                        std::cerr << (char)wide_content[i];
                    } else {
                        std::cerr << "\\x" << std::hex << (int)wide_content[i] << std::dec;
                    }
                }
                std::cerr << std::endl;
                #endif
            }
            wide_content.clear();
            wide_content.shrink_to_fit();
        } else {
            wide_content.clear();
            wide_content.shrink_to_fit();
            SetLastError(0);
            DWORD error = GetLastError();
            if (error != 0) {
                std::cerr << "[ERROR] Failed to convert file content to wide string, error: " << error << std::endl;
            } else {
                std::cerr << "[ERROR] Failed to convert file content to wide string" << std::endl;
            }
            SetWindowTextW(info_text, L"[Error: Could not convert file content to text]");
        }
        content.clear();
        content.shrink_to_fit();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in preview_file: " << e.what() << std::endl;
        SetWindowTextW(info_text, L"[Error reading file]");
    }
}

void GuiManager::set_status_text(const std::string& text) {
    if (status_bar) {
        try {
            int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
            std::vector<wchar_t> wide_text(len);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide_text.data(), len);
            SendMessageW(status_bar, SB_SETTEXT, 0, (LPARAM)wide_text.data());
            wide_text.clear();
            wide_text.shrink_to_fit();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in set_status_text: " << e.what() << std::endl;
        }
    }
}

void GuiManager::shutdown() {
    if (main_window) {
        DestroyWindow(main_window);
        main_window = nullptr;
    }

    if (hFont) {
        HFONT stockFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (hFont != stockFont) {
            DeleteObject(hFont);
        }
        hFont = nullptr;
    }

    UnregisterClassW(L"unPAKerWindowClass", GetModuleHandle(nullptr));
    parser = nullptr;
    Logger::instance().info("GUI shutdown complete");
}

void GuiManager::update() {
    if (!main_window) return;

    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            is_running = false;
            break;
        }

        if (msg.message == WM_KEYDOWN) {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (msg.wParam == 'O' || msg.wParam == 'o') {
                    handle_open_file();
                    continue;
                } else if (msg.wParam == 'Q' || msg.wParam == 'q') {
                    PostQuitMessage(0);
                    continue;
                }
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void GuiManager::render() {
    if (!main_window) return;

    UpdateWindow(main_window);

    Sleep(10);
}

bool GuiManager::should_close() const {
    return !is_running;
}

std::shared_ptr<FileEntry> GuiManager::find_file_in_tree(const std::shared_ptr<DirectoryEntry>& dir, const std::string& filename) {
    if (!dir) return nullptr;

    for (const auto& file : dir->files) {
        if (file && file->name == filename) {
            return file;
        }
    }

    for (const auto& subdir : dir->subdirectories) {
        auto result = find_file_in_tree(subdir, filename);
        if (result) return result;
    }

    return nullptr;
}

void GuiManager::check_for_updates() {
    if (!main_window) return;

    set_status_text("Checking for updates...");

    HINTERNET hSession = WinHttpOpen(
        L"unPAKer/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        std::wstring error_msg = L"Failed to initialize network connection.\n\nError code: ";
        error_msg += std::to_wstring(GetLastError());
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    DWORD timeout = 10000;
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        std::wstring error_msg = L"Failed to connect to GitHub API.\n\nError code: ";
        error_msg += std::to_wstring(GetLastError());
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    LPCWSTR acceptTypes[] = { L"application/json", nullptr };
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        L"/repos/mxtherfxcker/unPAKer/releases",
        nullptr,
        WINHTTP_NO_REFERER,
        acceptTypes,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::wstring error_msg = L"Failed to create API request.\n\nError code: ";
        error_msg += std::to_wstring(GetLastError());
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    LPCWSTR headers = L"User-Agent: unPAKer/1.0\r\nAccept: application/vnd.github.v3+json\r\n";

    BOOL bResult = WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::wstring error_msg = L"Failed to send API request.\n\nError code: ";
        error_msg += std::to_wstring(GetLastError());
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    bResult = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::wstring error_msg = L"Failed to receive API response.\n\nError code: ";
        error_msg += std::to_wstring(GetLastError());
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                               WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    DEBUG_COUT("[DEBUG] API request status code: " << statusCode << std::endl);
    DEBUG_COUT("[DEBUG] Request URL: /repos/mxtherfxcker/unPAKer/releases" << std::endl);

    if (statusCode != 200) {
        std::vector<char> errorBuffer;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;

        do {
            if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
            if (bytesAvailable == 0) break;

            size_t oldSize = errorBuffer.size();
            errorBuffer.resize(oldSize + bytesAvailable);
            if (!WinHttpReadData(hRequest, &errorBuffer[oldSize], bytesAvailable, &bytesRead)) break;
            if (bytesRead < bytesAvailable) {
                errorBuffer.resize(oldSize + bytesRead);
                break;
            }
        } while (bytesAvailable > 0);

        if (!errorBuffer.empty()) {
            std::string errorResponse(errorBuffer.begin(), errorBuffer.end());
            DEBUG_COUT("[DEBUG] Error response body: " << errorResponse.substr(0, 500) << std::endl);
        }
        errorBuffer.clear();
        errorBuffer.shrink_to_fit();

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::wstring error_msg = L"GitHub API returned error.\n\nStatus code: ";
        error_msg += std::to_wstring(statusCode);
        error_msg += L"\n\nURL: /repos/mxtherfxcker/unPAKer/releases";
        if (statusCode == 404) {
            error_msg += L"\n\nReleases not found. Check console for details.";
        }
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    std::vector<char> buffer;
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;

    do {
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) break;

        size_t oldSize = buffer.size();
        buffer.resize(oldSize + bytesAvailable);

        if (!WinHttpReadData(hRequest, &buffer[oldSize], bytesAvailable, &bytesRead)) {
            break;
        }

        if (bytesRead < bytesAvailable) {
            buffer.resize(oldSize + bytesRead);
            break;
        }
    } while (bytesAvailable > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (buffer.empty()) {
        MessageBoxW(main_window, L"Failed to read response from GitHub API.", L"Update Check Failed", MB_OK | MB_ICONERROR);
        set_status_text("Update check failed");
        buffer.clear();
        buffer.shrink_to_fit();
        return;
    }

    std::string jsonResponse(buffer.begin(), buffer.end());
    buffer.clear();
    buffer.shrink_to_fit();
    std::string latestVersion;

    std::string debugPreview = jsonResponse.substr(0, (jsonResponse.length() < 500) ? jsonResponse.length() : 500);
    DEBUG_COUT("[DEBUG] GitHub API response preview (first 500 chars):\n" << debugPreview << "\n" << std::endl);

    bool isArray = !jsonResponse.empty() && jsonResponse[0] == '[';

    if (!isArray) {
        MessageBoxW(main_window, L"Unexpected response format from GitHub API (expected array).", L"Update Check Failed", MB_OK | MB_ICONERROR);
        set_status_text("Update check failed");
        return;
    }

    std::regex tagRegex("\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
    std::regex prereleaseRegex("\"prerelease\"\\s*:\\s*(true|false)");
    std::smatch match;

    size_t pos = 0;
    bool foundStable = false;

    while (pos < jsonResponse.length()) {
        size_t objStart = jsonResponse.find('{', pos);
        if (objStart == std::string::npos) break;

        int braceCount = 0;
        size_t objEnd = objStart;
        for (size_t i = objStart; i < jsonResponse.length(); i++) {
            if (jsonResponse[i] == '{') braceCount++;
            if (jsonResponse[i] == '}') {
                braceCount--;
                if (braceCount == 0) {
                    objEnd = i + 1;
                    break;
                }
            }
        }

        if (objEnd <= objStart) break;

        std::string releaseObj = jsonResponse.substr(objStart, objEnd - objStart);

        bool isPrerelease = false;
        if (std::regex_search(releaseObj, match, prereleaseRegex)) {
            isPrerelease = (match[1].str() == "true");
        }

        if (std::regex_search(releaseObj, match, tagRegex) && match.size() > 1) {
            std::string tagName = match[1].str();

            if (!isPrerelease) {
                latestVersion = tagName;
                foundStable = true;
                DEBUG_COUT("[DEBUG] Found stable release: " << latestVersion << std::endl);
                break;
            } else if (!foundStable) {
                latestVersion = tagName;
                DEBUG_COUT("[DEBUG] Found prerelease (storing as fallback): " << latestVersion << std::endl);
            }
        }

        pos = objEnd;
    }

    bool found = !latestVersion.empty();

    if (!found) {
        std::wstring error_msg = L"Failed to parse version information from GitHub API response.\n\n";
        error_msg += L"Response preview (first 200 chars):\n";
        std::string preview = jsonResponse.substr(0, (jsonResponse.length() < 200) ? jsonResponse.length() : 200);
        std::wstring wPreview(preview.begin(), preview.end());
        error_msg += wPreview;
        MessageBoxW(main_window, error_msg.c_str(), L"Update Check Failed", MB_OK | MB_ICONERROR);
        error_msg.clear();
        error_msg.shrink_to_fit();
        preview.clear();
        preview.shrink_to_fit();
        wPreview.clear();
        wPreview.shrink_to_fit();
        set_status_text("Update check failed");
        return;
    }

    std::string currentVersion = GITHUB_VERSION;

    std::string normalizedCurrent = currentVersion;
    std::string normalizedLatest = latestVersion;

    if (!normalizedCurrent.empty() && normalizedCurrent[0] == 'v') {
        normalizedCurrent = normalizedCurrent.substr(1);
    }
    if (!normalizedLatest.empty() && normalizedLatest[0] == 'v') {
        normalizedLatest = normalizedLatest.substr(1);
    }

    bool isUpToDate = false;

    std::regex versionRegex(R"((\d+)\.(\d+)(?:\.(\d+))?(?:-([\w]+)(\d*))?)");
    std::smatch currentMatch, latestMatch;

    if (std::regex_match(normalizedCurrent, currentMatch, versionRegex) &&
        std::regex_match(normalizedLatest, latestMatch, versionRegex)) {

        int currentMajor = std::stoi(currentMatch[1].str());
        int latestMajor = std::stoi(latestMatch[1].str());

        if (currentMajor < latestMajor) {
            isUpToDate = false;
        } else if (currentMajor > latestMajor) {
            isUpToDate = true;
        } else {
            int currentMinor = std::stoi(currentMatch[2].str());
            int latestMinor = std::stoi(latestMatch[2].str());

            if (currentMinor < latestMinor) {
                isUpToDate = false;
            } else if (currentMinor > latestMinor) {
                isUpToDate = true;
            } else {
                std::string currentPatchStr = currentMatch[3].str();
                std::string latestPatchStr = latestMatch[3].str();
                int currentPatch = currentPatchStr.empty() ? 0 : std::stoi(currentPatchStr);
                int latestPatch = latestPatchStr.empty() ? 0 : std::stoi(latestPatchStr);

                if (currentPatch < latestPatch) {
                    isUpToDate = false;
                } else if (currentPatch > latestPatch) {
                    isUpToDate = true;
                } else {
                    std::string currentSuffix = currentMatch[4].str();
                    std::string latestSuffix = latestMatch[4].str();

                    if (currentSuffix.empty() && latestSuffix.empty()) {
                        isUpToDate = true;
                    } else if (currentSuffix.empty() && !latestSuffix.empty()) {
                        isUpToDate = true;
                    } else if (!currentSuffix.empty() && latestSuffix.empty()) {
                        isUpToDate = false;
                    } else {
                        auto getSuffixPriority = [](const std::string& suffix) -> int {
                            if (suffix == "stable") return 5;
                            if (suffix.find("rc") == 0) return 4;
                            if (suffix.find("beta") == 0) return 3;
                            if (suffix.find("alpha") == 0) return 2;
                            if (suffix.find("dev") == 0) return 1;
                            return 0;
                        };

                        int currentPriority = getSuffixPriority(currentSuffix);
                        int latestPriority = getSuffixPriority(latestSuffix);

                        if (currentPriority < latestPriority) {
                            isUpToDate = false;
                        } else if (currentPriority > latestPriority) {
                            isUpToDate = true;
                        } else {
                            std::string currentSuffixNum = currentMatch[5].str();
                            std::string latestSuffixNum = latestMatch[5].str();

                            if (currentSuffixNum.empty() && latestSuffixNum.empty()) {
                                isUpToDate = true;
                            } else if (currentSuffixNum.empty()) {
                                isUpToDate = true;
                            } else if (latestSuffixNum.empty()) {
                                isUpToDate = false;
                            } else {
                                int currentNum = std::stoi(currentSuffixNum);
                                int latestNum = std::stoi(latestSuffixNum);
                                isUpToDate = (currentNum >= latestNum);
                            }
                        }
                    }
                }
            }
        }
    } else {
        isUpToDate = (normalizedCurrent == normalizedLatest);
    }

    std::wstring message;
    std::wstring title;
    UINT icon;

    if (isUpToDate) {
        message = L"You are using the latest version.\n\n";
        message += L"Current version: ";
        std::wstring wCurrent(currentVersion.begin(), currentVersion.end());
        message += wCurrent;
        message += L"\nLatest version: ";
        std::wstring wLatest(latestVersion.begin(), latestVersion.end());
        message += wLatest;
        title = L"Update Check";
        icon = MB_OK | MB_ICONINFORMATION;
        set_status_text("You are using the latest version");
        currentVersion.clear();
        currentVersion.shrink_to_fit();
    } else {
        message = L"A new version is available!\n\n";
        message += L"Current version: v";
        std::wstring wCurrent(currentVersion.begin(), currentVersion.end());
        message += wCurrent;
        message += L"\nLatest version: ";
        std::wstring wLatest(latestVersion.begin(), latestVersion.end());
        message += wLatest;
        message += L"\n\nYou can download it from:\nhttps://github.com/mxtherfxcker/unPAKer/releases/";
        title = L"Update Available";
        icon = MB_OK | MB_ICONINFORMATION;
        set_status_text("New version available: " + latestVersion);
        currentVersion.clear();
        currentVersion.shrink_to_fit();
    }

    MessageBoxW(main_window, message.c_str(), title.c_str(), icon);
    message.clear();
    message.shrink_to_fit();
    jsonResponse.clear();
    jsonResponse.shrink_to_fit();
    latestVersion.clear();
    latestVersion.shrink_to_fit();
}

} // namespace unpaker
