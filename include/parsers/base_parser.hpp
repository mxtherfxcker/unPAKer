// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include "pak_parser.hpp"
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace unpaker::parsers {

class BaseParser {
public:
    virtual ~BaseParser() = default;

    virtual bool parse(const fs::path& archive_path,
                                              std::shared_ptr<DirectoryEntry>& root,
                                              uint32_t& file_count) = 0;

    virtual bool detect(const fs::path& archive_path) = 0;

    virtual bool extract_file(const fs::path& archive_path,
                                                         const std::shared_ptr<FileEntry>& file,
                                                         std::vector<uint8_t>& data) const = 0;
};

} // namespace unpaker::parsers
