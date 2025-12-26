// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#include "generic_parser.hpp"
#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <vector>

namespace unpaker::parsers {

bool GenericParser::detect(const fs::path&) {
    Logger::instance().info("Generic Parser: Using as fallback parser");
    return true;
}

bool GenericParser::parse(const fs::path&,
                                                 std::shared_ptr<DirectoryEntry>&,
                                                 uint32_t&) {
    Logger::instance().error("Generic: No specific parser matched this archive format");
    return false;
}

bool GenericParser::extract_file(const fs::path& archive_path,
                                const std::shared_ptr<FileEntry>& file,
                                std::vector<uint8_t>& data) const {
    if (!file) {
        std::cerr << "[ERROR] Generic: Invalid file entry" << std::endl;
        return false;
    }

    try {
        std::ifstream ifs(archive_path, std::ios::binary);
        if (!ifs.is_open()) {
            std::cerr << "[ERROR] Generic: Failed to open archive file: " << archive_path.string() << std::endl;
            return false;
        }

        ifs.seekg(file->offset, std::ios::beg);
        if (!ifs.good()) {
            std::cerr << "[ERROR] Generic: Failed to seek to file offset: " << file->offset << std::endl;
            return false;
        }

        data.resize(file->size);
        ifs.read(reinterpret_cast<char*>(data.data()), file->size);

        if (!ifs.good() && !ifs.eof()) {
            std::cerr << "[ERROR] Generic: Failed to read file data" << std::endl;
            return false;
        }

        ifs.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Generic: Exception in extract_file: " << e.what() << std::endl;
        return false;
    }
}

} // namespace unpaker::parsers
