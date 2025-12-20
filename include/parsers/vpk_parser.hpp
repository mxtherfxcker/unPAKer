// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include "base_parser.hpp"
#include <fstream>

namespace unpaker::parsers {

class VpkParser : public BaseParser {
public:
    bool parse(const fs::path& archive_path,
                              std::shared_ptr<DirectoryEntry>& root,
                              uint32_t& file_count) override;

    bool detect(const fs::path& archive_path) override;

    bool extract_file(const fs::path& archive_path,
                                         const std::shared_ptr<FileEntry>& file,
                                         std::vector<uint8_t>& data) const override;

private:
    bool parse_vpk_v2(std::ifstream& file,
                                         std::shared_ptr<DirectoryEntry>& root,
                                         uint32_t& file_count);

    bool parse_vpk_dir(std::ifstream& file,
                                              std::shared_ptr<DirectoryEntry>& root,
                                              uint32_t& file_count);

    void build_directory_structure(std::shared_ptr<DirectoryEntry>& root);

    std::string read_cstring(std::ifstream& file);

    bool read_from_data_file(const fs::path& data_file_path,
                                                         const std::shared_ptr<FileEntry>& file,
                                                         std::vector<uint8_t>& data) const;

    bool fallback_search_data_archives(const fs::path& archive_path,
                                                                               const std::shared_ptr<FileEntry>& file,
                                                                               std::vector<uint8_t>& data) const;
};

} // namespace unpaker::parsers
