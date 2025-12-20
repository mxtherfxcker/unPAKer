// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include "base_parser.hpp"

namespace unpaker::parsers {

class GenericParser : public BaseParser {
public:
    bool parse(const fs::path& archive_path,
                              std::shared_ptr<DirectoryEntry>& root,
                              uint32_t& file_count) override;

    bool detect(const fs::path& archive_path) override;

    bool extract_file(const fs::path& archive_path,
                                         const std::shared_ptr<FileEntry>& file,
                                         std::vector<uint8_t>& data) const override;
};

} // namespace unpaker::parsers
