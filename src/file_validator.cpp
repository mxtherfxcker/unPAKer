// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#include "file_validator.hpp"
#include "logger.hpp"
#include <iostream>
#include <algorithm>

namespace unpaker {

ValidationResult FileValidator::validateArchive(const std::shared_ptr<DirectoryEntry>& root,
                                                                                               uint64_t archive_size) {
    ValidationResult result;

    if (!root) {
        result.is_valid = false;
        result.error_messages.push_back("Root directory is null");
        return result;
    }

    std::vector<std::shared_ptr<FileEntry>> all_files;
    collectAllFiles(root, all_files);

    result.total_files = static_cast<uint32_t>(all_files.size());

    Logger::instance().info(std::string("Validating archive with ") + std::to_string(result.total_files) + " files...");

    std::vector<std::string> duplicates;
    result.duplicate_files = checkDuplicates(all_files, duplicates);
    if (result.duplicate_files > 0) {
        result.warnings.push_back("Found " + std::to_string(result.duplicate_files) + " duplicate file entries");
        for (const auto& dup : duplicates) {
            result.warnings.push_back("  Duplicate: " + dup);
        }
    }

    for (const auto& file : all_files) {
        if (!validateFileEntry(file, archive_size)) {
            result.invalid_entries++;
            result.is_valid = false;
            result.error_messages.push_back("Invalid entry: " + file->path);
        }


        if (file->size == 0 && !file->is_directory) {
            result.zero_size_files++;
            result.warnings.push_back("Zero-size file: " + file->path);
        }
    }

    if (result.error_messages.empty()) {
        Logger::instance().success("Archive validation passed");
    } else {
        Logger::instance().warning(std::string("Archive validation found ") + std::to_string(result.error_messages.size()) + " errors:");
        for (const auto& error : result.error_messages) {
            Logger::instance().error(error);
        }
    }

    if (!result.warnings.empty()) {
        Logger::instance().info(std::string("Found ") + std::to_string(result.warnings.size()) + " warnings:");
        for (const auto& warning : result.warnings) {
            Logger::instance().warning(warning);
        }
    }

    return result;
}

uint32_t FileValidator::checkDuplicates(const std::vector<std::shared_ptr<FileEntry>>& files,
                                                                               std::vector<std::string>& duplicates) {
    std::set<std::string> seen;
    uint32_t dup_count = 0;

    for (const auto& file : files) {
        if (!file) continue;

        if (seen.count(file->path) > 0) {
            dup_count++;
            duplicates.push_back(file->path);
        } else {
            seen.insert(file->path);
        }
    }

    return dup_count;
}

bool FileValidator::validateFileEntry(const std::shared_ptr<FileEntry>& entry,
                                                                         uint64_t) {
    if (!entry) return false;

    if (entry->path.empty()) {
        std::cerr << "[WARNING] File entry has empty path" << std::endl;
        return false;
    }

    if (entry->path.length() > 1024) {
        std::cerr << "[WARNING] File path exceeds 1024 characters: " << entry->path.length() << std::endl;
        return false;
    }

    for (size_t i = 0; i < entry->path.length(); ++i) {
        unsigned char c = entry->path[i];
        if (c < 32 || c > 126) {
            if (c != '/' && c != '\\') {
                std::cerr << "[WARNING] File path contains invalid character (0x" << std::hex
                                                  << (int)c << std::dec << ") in: " << entry->path << std::endl;
                return false;
            }
        }
    }


    return true;
}

void FileValidator::collectAllFiles(const std::shared_ptr<DirectoryEntry>& dir,
                                                                       std::vector<std::shared_ptr<FileEntry>>& files) {
    if (!dir) return;

    for (const auto& file : dir->files) {
        if (file) {
            files.push_back(file);
        }
    }

    for (const auto& subdir : dir->subdirectories) {
        if (subdir) {
            collectAllFiles(subdir, files);
        }
    }
}

} // namespace unpaker
