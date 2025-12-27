// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#include "ue_parser.hpp"
#include "logger.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <vector>

namespace unpaker::parsers {

bool UEParser::detect(const fs::path& archive_path) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) return false;

    char magic[4];
    file.read(magic, 4);

    if (std::memcmp(magic, "\x50\x61\x6B\x00", 4) == 0) {
        Logger::instance().info("UE Parser: Detected Unreal Engine 3 format");
        return true;
    }

    if (magic[0] == 'P' && magic[1] == 'A' && magic[2] == 'K') {
        Logger::instance().info("UE Parser: Detected Unreal Engine 4/5 format");
        return true;
    }

    return false;
}

std::string UEParser::read_cstring(std::FILE* file, size_t offset) {
    std::fseek(file, static_cast<long>(offset), SEEK_SET);
    std::string result;
    const size_t MAX_STRING_LEN = 512;

    char ch;
    while (std::fread(&ch, 1, 1, file) == 1 && ch != '\0' && result.length() < MAX_STRING_LEN) {
        result += ch;
    }

    return result;
}

bool UEParser::parse(const fs::path& archive_path,
                    std::shared_ptr<DirectoryEntry>& root,
                    uint32_t& file_count) {
    FILE* file;
    errno_t err = fopen_s(&file, archive_path.string().c_str(), "rb");
    if (err != 0 || !file) {
        std::cerr << "[ERROR] UE: Cannot open file: " << archive_path.string() << std::endl;
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    uint64_t file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size < 36) {
        std::cerr << "[ERROR] UE: File too small" << std::endl;
        std::fclose(file);
        return false;
    }

    char magic[4];
    std::fread(magic, 1, 4, file);

    Logger::instance().info("UE: Parsing Unreal Engine PAK format");

    uint32_t version;
    std::fread(&version, 4, 1, file);

    std::fseek(file, static_cast<long>(file_size - 4), SEEK_SET);
    uint32_t entry_count;
    std::fread(&entry_count, 4, 1, file);

    DEBUG_COUT("[DEBUG] UE: Entry count from header: " << entry_count << std::endl);
    DEBUG_COUT("[DEBUG] UE: File size: " << file_size << " bytes" << std::endl);

    if (entry_count > 100000) {
        Logger::instance().warning(std::string("UE: Suspicious entry count: ") + std::to_string(entry_count) + std::string(", adjusting to 256"));
        entry_count = 256;
    }

    std::fseek(file, 4, SEEK_SET);

    uint32_t file_count_local = 0;
    for (uint32_t i = 0; i < entry_count; ++i) {
        uint32_t path_len = 0;
        if (std::fread(&path_len, 4, 1, file) != 1) {
            DEBUG_COUT("[DEBUG] UE: Reached end of entries at entry " << i << std::endl);
            break;
        }

        if (path_len == 0 || path_len > 512) {
            DEBUG_COUT("[DEBUG] UE: Invalid path string at entry " << i << std::endl);
            break;
        }

        char path[512] = {0};
        if (std::fread(path, 1, path_len, file) != path_len) {
            break;
        }

        uint64_t offset = 0;
        uint64_t size = 0;
        std::fread(&offset, 8, 1, file);
        std::fread(&size, 8, 1, file);

        if (offset > file_size || size > file_size) {
            continue;
        }

        auto entry = std::make_shared<FileEntry>();
        if (!entry) {
            std::cerr << "[ERROR] UE: Memory allocation failed" << std::endl;
            std::fclose(file);
            return false;
        }

        entry->name = path;
        entry->path = path;
        entry->offset = static_cast<uint32_t>(offset);
        entry->size = static_cast<uint32_t>(size);
        entry->archive_index = 0x7fff;
        entry->is_directory = false;

        if (!entry->name.empty()) {
            root->files.push_back(entry);
            file_count++;
            file_count_local++;
        }
    }

    Logger::instance().info(std::string("UE: Successfully parsed ") + std::to_string(file_count_local) + std::string(" file entries"));

    std::fclose(file);
    return file_count_local > 0;
}

bool UEParser::extract_file(const fs::path& archive_path,
                            const std::shared_ptr<FileEntry>& file,
                            std::vector<uint8_t>& data) const {
    if (!file) {
        std::cerr << "[ERROR] UE: Invalid file entry" << std::endl;
        return false;
    }

    try {
        std::ifstream ifs(archive_path, std::ios::binary);
        if (!ifs.is_open()) {
            std::cerr << "[ERROR] UE: Failed to open archive file: " << archive_path.string() << std::endl;
            return false;
        }

        ifs.seekg(file->offset, std::ios::beg);
        if (!ifs.good()) {
            std::cerr << "[ERROR] UE: Failed to seek to file offset: " << file->offset << std::endl;
            return false;
        }

        data.resize(file->size);
        ifs.read(reinterpret_cast<char*>(data.data()), file->size);

        if (!ifs.good() && !ifs.eof()) {
            std::cerr << "[ERROR] UE: Failed to read file data" << std::endl;
            return false;
        }

        ifs.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] UE: Exception in extract_file: " << e.what() << std::endl;
        return false;
    }
}

} // namespace unpaker::parsers
