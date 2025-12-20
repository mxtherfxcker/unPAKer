// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#include "pak_parser.hpp"
#include "parsers/vpk_parser.hpp"
#include "parsers/ue_parser.hpp"
#include "parsers/generic_parser.hpp"
#include <iostream>
#include <cstring>
#include <fstream>

namespace unpaker {

PakParser::PakParser(const fs::path& pak_path)
    : archive_path(pak_path),
              detected_format(PakFormat::UNKNOWN),
              file_count(0),
              archive_size(0),
              current_parser(nullptr) {
    root_directory = std::make_shared<DirectoryEntry>();
    if (!root_directory) {
        std::cerr << "[ERROR] Failed to allocate memory for root directory" << std::endl;
        return;
    }

    root_directory->name = "/";
    root_directory->is_directory = true;
    root_directory->parent = nullptr;

    if (fs::exists(pak_path)) {
        try {
            archive_size = fs::file_size(pak_path);
            std::cout << "[INFO] Archive file size: " << (archive_size / (1024.0 * 1024.0)) << " MB" << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[ERROR] Failed to get file size: " << e.what() << std::endl;
            archive_size = 0;
        }
    } else {
        std::cerr << "[ERROR] Archive file does not exist: " << pak_path.string() << std::endl;
    }
}

PakParser::~PakParser() = default;

bool PakParser::detect_format() {
    parsers::VpkParser vpk_parser;
    if (vpk_parser.detect(archive_path)) {
        detected_format = PakFormat::SOURCE_ENGINE;
        current_parser = std::make_shared<parsers::VpkParser>();
        return true;
    }

    parsers::UEParser ue_parser;
    if (ue_parser.detect(archive_path)) {
        std::ifstream file(archive_path, std::ios::binary);
        if (file) {
            char magic[4];
            file.read(magic, 4);

            if (std::memcmp(magic, "\x50\x61\x6B\x00", 4) == 0) {
                detected_format = PakFormat::UNREAL_ENGINE_3;
            } else {
                detected_format = PakFormat::UNREAL_ENGINE_4_5;
            }
        }
        current_parser = std::make_shared<parsers::UEParser>();
        return true;
    }

    return false;
}

bool PakParser::parse() {
    std::cout << "[INFO] Attempting to parse archive..." << std::endl;

    root_directory = std::make_shared<DirectoryEntry>();
    if (!root_directory) {
        std::cerr << "[ERROR] Failed to allocate root directory" << std::endl;
        return false;
    }
    root_directory->name = archive_path.filename().string();
    root_directory->is_directory = true;
    root_directory->parent = nullptr;
    file_count = 0;

    if (!detect_format()) {
        std::cout << "[WARNING] Could not detect format, trying generic parser..." << std::endl;
        detected_format = PakFormat::GENERIC;
        current_parser = std::make_shared<parsers::GenericParser>();
    }

    std::cout << "[INFO] Detected format: " << get_format_info() << std::endl;

    bool parse_result = false;

    if (current_parser) {
        parse_result = current_parser->parse(archive_path, root_directory, file_count);
    } else {
        std::cerr << "[ERROR] No parser available" << std::endl;
        return false;
    }

    if (parse_result) {
        std::cout << "[SUCCESS] Archive parsed successfully" << std::endl;
        if (file_count == 0) {
            std::cout << "[WARNING] No entries found. This might be a file list or metadata file" << std::endl;
        }
    } else {
        std::cout << "[ERROR] Failed to parse archive" << std::endl;
    }

    return parse_result;
}

bool PakParser::build_directory_tree() {
    for (const auto& file : root_directory->files) {
        std::string path = file->path;
        auto current = root_directory;

        size_t pos = 0;
        while ((pos = path.find_first_of("/\\", pos)) != std::string::npos) {
            std::string dir_name = path.substr(0, pos);

            auto it = std::find_if(
                current->subdirectories.begin(),
                current->subdirectories.end(),
                [&dir_name](const auto& d) { return d->name == dir_name; }
            );

            if (it == current->subdirectories.end()) {
                auto new_dir = std::make_shared<DirectoryEntry>();
                new_dir->name = dir_name;
                new_dir->parent = current;
                current->subdirectories.push_back(new_dir);
                current = new_dir;
            } else {
                current = *it;
            }

            path = path.substr(pos + 1);
            pos = 0;
        }
    }

    return true;
}

std::shared_ptr<DirectoryEntry> PakParser::get_root() const {
    return root_directory;
}

bool PakParser::is_valid() const {
    return file_count > 0;
}

std::string PakParser::get_format_info() const {
    switch (detected_format) {
        case PakFormat::UNREAL_ENGINE_3:
            return "Unreal Engine 3";
        case PakFormat::UNREAL_ENGINE_4_5:
            return "Unreal Engine 4/5";
        case PakFormat::SOURCE_ENGINE:
            return "Source Engine";
        case PakFormat::GENERIC:
            return "Generic PAK";
        case PakFormat::UNKNOWN:
        default:
            return "Unknown";
    }
}

uint32_t PakParser::get_file_count() const {
    return file_count;
}

uint64_t PakParser::get_archive_size() const {
    return archive_size;
}

bool PakParser::extract_file(const std::shared_ptr<FileEntry>& file, std::vector<uint8_t>& data) const {
    if (!file || !current_parser) {
        std::cerr << "[ERROR] Invalid file or no parser available" << std::endl;
        return false;
    }

    try {
        return current_parser->extract_file(archive_path, file, data);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in extract_file: " << e.what() << std::endl;
        return false;
    }
}


}
