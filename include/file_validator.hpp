// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include "pak_parser.hpp"
#include <set>
#include <string>
#include <vector>

namespace unpaker {

struct ValidationResult {
    bool is_valid = true;
    uint32_t total_files = 0;
    uint32_t duplicate_files = 0;
    uint32_t invalid_entries = 0;
    uint32_t invalid_offsets = 0;
    uint32_t zero_size_files = 0;
    std::vector<std::string> error_messages;
    std::vector<std::string> warnings;
};

class FileValidator {
public:
    static ValidationResult validateArchive(const std::shared_ptr<DirectoryEntry>& root,
                                                                                       uint64_t archive_size);

    static uint32_t checkDuplicates(const std::vector<std::shared_ptr<FileEntry>>& files,
                                    std::vector<std::string>& duplicates);

    static bool validateFileEntry(const std::shared_ptr<FileEntry>& entry,
                                                                  uint64_t archive_size);

private:
    static void collectAllFiles(const std::shared_ptr<DirectoryEntry>& dir,
                                                               std::vector<std::shared_ptr<FileEntry>>& files);
};

} // namespace unpaker
