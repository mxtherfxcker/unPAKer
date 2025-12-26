// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace unpaker {

struct FileEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    std::string path;
    bool is_directory;
    uint32_t archive_index;
};

struct DirectoryEntry {
    std::string name;
    std::vector<std::shared_ptr<FileEntry>> files;
    std::vector<std::shared_ptr<DirectoryEntry>> subdirectories;
    std::shared_ptr<DirectoryEntry> parent;
    bool is_directory = true;
};

namespace parsers {
    class BaseParser;
}

class PakParser {
public:
    explicit PakParser(const fs::path& pak_path);
    ~PakParser();

    bool parse();
    std::shared_ptr<DirectoryEntry> get_root() const;
    bool is_valid() const;
    std::string get_format_info() const;
    uint32_t get_file_count() const;
    uint64_t get_archive_size() const;
    bool extract_file(const std::shared_ptr<FileEntry>& file, std::vector<uint8_t>& data) const;

private:
    enum class PakFormat {
        UNKNOWN,
        UNREAL_ENGINE_3,
        UNREAL_ENGINE_4_5,
        SOURCE_ENGINE,
        GENERIC
    };

    fs::path archive_path;
    std::shared_ptr<DirectoryEntry> root_directory;
    PakFormat detected_format;
    uint32_t file_count;
    uint64_t archive_size;
    std::shared_ptr<parsers::BaseParser> current_parser;

    bool detect_format();
    bool build_directory_tree();
};

}
