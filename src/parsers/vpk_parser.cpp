// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#include "vpk_parser.hpp"
#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <map>
#include <vector>
#include <utility>

namespace unpaker::parsers {

bool VpkParser::detect(const fs::path& archive_path) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) return false;

    char magic[4];
    file.read(magic, 4);

    uint32_t magic_int;
    std::memcpy(&magic_int, magic, sizeof(uint32_t));

    std::ostringstream oss;
    oss << "VPK: File signature: 0x" << std::hex << magic_int << std::dec;
    LOG_DEBUG(oss.str());

    if (magic_int == 0x55aa1234) {
        std::cout << "[INFO] VPK Parser: Detected VPK v2 archive format" << std::endl;
        return true;
    }

    if (magic_int == 0x465456) {
        std::cout << "[INFO] VPK Parser: Detected VPK directory format" << std::endl;
        return true;
    }

    return false;
}

std::string VpkParser::read_cstring(std::ifstream& file) {
    std::string result;
    char ch = '\0';
    const size_t MAX_STRING_LEN = 256;

    while (result.length() < MAX_STRING_LEN) {
        if (!file.get(ch)) {
            std::cerr << "[ERROR] VPK: Unexpected end of file while reading string at offset "
                                              << file.tellg() << std::endl;
            return "\x01[READ_ERROR]\x01";
        }

        if (ch == '\0') {
            break;
        }

        result += ch;
    }

    if (result.length() >= MAX_STRING_LEN && ch != '\0') {
        std::cerr << "[ERROR] VPK: String exceeded max length (" << MAX_STRING_LEN
                                  << " chars) at offset " << (file.tellg() - static_cast<std::streamoff>(1)) << std::endl;
        size_t skip_count = 0;
        const size_t MAX_SKIP = 1000;
        while (skip_count < MAX_SKIP) {
            if (!file.get(ch)) {
                std::cerr << "[ERROR] VPK: End of file while skipping overflow string" << std::endl;
                return "\x01[OVERFLOW_EOF]\x01";
            }
            if (ch == '\0') {
                break;
            }
            skip_count++;
        }
        if (skip_count >= MAX_SKIP) {
            std::cerr << "[ERROR] VPK: Could not find null terminator after " << MAX_SKIP
                                              << " characters" << std::endl;
            return "\x01[OVERFLOW_NO_NULL]\x01";
        }
        return "\x01[OVERFLOW]\x01";
    }

    return result;
}

bool VpkParser::parse_vpk_v2(std::ifstream& file,
                                                         std::shared_ptr<DirectoryEntry>& root,
                                                         uint32_t& file_count) {
    std::cout << "[INFO] VPK: Parsing v2 archive format" << std::endl;

    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();
    file.seekg(4); // Skip signature

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);

    uint32_t tree_size = 0;
    file.read(reinterpret_cast<char*>(&tree_size), 4);

    std::cout << "[INFO] VPK: Version=" << version << ", TreeSize=" << tree_size << std::endl;

    uint32_t tree_offset = 12;
    if (version == 2) {
        uint32_t file_data_section_size = 0;
        file.read(reinterpret_cast<char*>(&file_data_section_size), 4);

        uint32_t tree_crc = 0, file_crc = 0, meta_crc = 0;
        file.read(reinterpret_cast<char*>(&tree_crc), 4);
        file.read(reinterpret_cast<char*>(&file_crc), 4);
        file.read(reinterpret_cast<char*>(&meta_crc), 4);

        tree_offset = 28;
        std::cout << "[INFO] VPK: FileDataSectionSize=" << file_data_section_size
                                  << ", TreeCRC=0x" << std::hex << tree_crc
                                  << ", FileCRC=0x" << file_crc
                                  << ", MetaCRC=0x" << meta_crc << std::dec << std::endl;
    }

    file.seekg(tree_offset);

    if (tree_size > static_cast<uint32_t>(file_size - tree_offset)) {
        std::cerr << "[WARNING] VPK: tree_size (" << tree_size
                                  << ") exceeds available file space (" << (file_size - tree_offset)
                                  << "), clamping to file size" << std::endl;
        tree_size = static_cast<uint32_t>(file_size - tree_offset);
    }

    std::streampos tree_end_pos = tree_offset + tree_size;

    DEBUG_COUT("[DEBUG] VPK: Tree starts at offset " << tree_offset
                              << ", tree_size=" << tree_size << ", tree should end at offset " << tree_end_pos
                              << ", file size=" << file_size << std::endl);

    uint32_t file_count_local = 0;

    auto is_error_marker = [](const std::string& s) {
        return s.find("[OVERFLOW]") != std::string::npos || s.find("[ERROR") != std::string::npos;
    };

    while (file.tellg() < tree_end_pos && file.good()) {
        std::string ext_name = read_cstring(file);

        if (file.tellg() > tree_end_pos) break;

        if (is_error_marker(ext_name)) {
            std::cerr << "[ERROR] VPK: Found error marker in extension name, stopping parse" << std::endl;
            break;
        }

        if (ext_name.empty()) {
            uint16_t term_check = 0;
            std::streampos current = file.tellg();
            if (current + static_cast<std::streamoff>(2) > tree_end_pos) break;
            file.read(reinterpret_cast<char*>(&term_check), 2);
            if (term_check == 0xffff) {
                DEBUG_COUT("[DEBUG] VPK: Tree parsing completed successfully" << std::endl);
                break;
            }
            file.seekg(-2, std::ios_base::cur);
            continue;
        }

        DEBUG_COUT("[DEBUG] VPK: Extension: " << ext_name << std::endl);

        while (file.tellg() < tree_end_pos && file.good()) {
            std::string dir_name = read_cstring(file);

            if (file.tellg() > tree_end_pos) break;

            if (is_error_marker(dir_name)) {
                std::cerr << "[ERROR] VPK: Found error marker in directory name, aborting extension" << std::endl;
                break;
            }

            if (dir_name.empty()) {
                break;
            }

            while (file.tellg() < tree_end_pos && file.good()) {
                std::streampos before_read = file.tellg();

                std::string file_name = read_cstring(file);
                std::streampos after_read = file.tellg();

                if (after_read <= before_read && !file_name.empty()) {
                    std::cerr << "[ERROR] VPK: File position did not advance after reading file name at offset "
                                                              << before_read << std::endl;
                    break;
                }

                if (file.tellg() > tree_end_pos) {
                    std::cerr << "[WARNING] VPK: Read past tree boundary at offset " << file.tellg() << std::endl;
                    break;
                }

                if (is_error_marker(file_name)) {
                    std::cerr << "[ERROR] VPK: Found error marker in file name, aborting directory" << std::endl;
                    break;
                }

                if (file_name.empty()) {
                    break;
                }

                if (file_name.length() > 0 && (file_name[0] == '+' || file_name[0] == '{' || file_name[0] == '~')) {
                    DEBUG_CERR("[DEBUG] VPK: Reading suspicious file name \"" << file_name
                                                              << "\" at offset " << before_read
                                                              << ", after read at " << after_read
                                                              << ", bytes read: " << (after_read - before_read)
                                                              << ", expected: " << (file_name.length() + 1) << std::endl);

                    std::streamoff expected_offset = before_read + static_cast<std::streamoff>(file_name.length() + 1);
                    if (after_read != expected_offset) {
                        std::cerr << "[ERROR] VPK: Position mismatch! Expected " << expected_offset
                                                                  << " but got " << after_read
                                                                  << " (difference: " << (after_read - expected_offset) << " bytes)" << std::endl;
                        std::cerr << "[ERROR] VPK: This indicates the string was not read correctly - "
                                                                  << "possibly missing or extra null terminator" << std::endl;
                        std::streampos saved_pos = file.tellg();
                        char check_bytes[20];
                        file.read(check_bytes, 20);
                        file.seekg(saved_pos);
                        DEBUG_CERR("[DEBUG] VPK: Bytes at current position (hex): ");
                        #ifdef _DEBUG
                        for (int i = 0; i < 20; i++) {
                            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                                                                              << (unsigned char)check_bytes[i] << " ";
                        }
                        std::cerr << std::dec << std::endl;
                        #endif

                        file.seekg(expected_offset);
                    }
                }

                std::streampos current_pos = file.tellg();
                if (current_pos + static_cast<std::streamoff>(18) > tree_end_pos) {
                    std::cerr << "[WARNING] VPK: Not enough space for file metadata at "
                                                              << current_pos << " (need 18 bytes, have "
                                                              << (tree_end_pos - current_pos) << "), stopping parse" << std::endl;
                    file.seekg(tree_end_pos);
                    break;
                }

                std::streampos before_metadata = file.tellg();

                uint32_t crc = 0;
                uint32_t archive_index = 0;
                uint32_t entry_offset = 0;
                uint32_t entry_size = 0;

                if (!file.read(reinterpret_cast<char*>(&crc), 4)) {
                    std::cerr << "[ERROR] VPK: Failed to read CRC at offset " << file.tellg() << std::endl;
                    break;
                }
                if (!file.read(reinterpret_cast<char*>(&archive_index), 4)) {
                    std::cerr << "[ERROR] VPK: Failed to read archive_index at offset " << file.tellg() << std::endl;
                    break;
                }
                if (!file.read(reinterpret_cast<char*>(&entry_offset), 4)) {
                    std::cerr << "[ERROR] VPK: Failed to read entry_offset at offset " << file.tellg() << std::endl;
                    break;
                }
                if (!file.read(reinterpret_cast<char*>(&entry_size), 4)) {
                    std::cerr << "[ERROR] VPK: Failed to read entry_size at offset " << file.tellg() << std::endl;
                    break;
                }

                uint16_t term_flag = 0;
                if (!file.read(reinterpret_cast<char*>(&term_flag), 2)) {
                    std::cerr << "[ERROR] VPK: Failed to read terminator flag at offset " << file.tellg() << std::endl;
                    break;
                }

                std::streampos after_metadata = file.tellg();

                std::streamoff metadata_bytes_read = after_metadata - before_metadata;
                if (metadata_bytes_read != 18) {
                    std::cerr << "[ERROR] VPK: Metadata read error! Expected 18 bytes, but read "
                                                              << metadata_bytes_read << " bytes at offset " << before_metadata << std::endl;
                    break;
                }

                if (term_flag == 0xffff) {
                    std::streampos current_file_pos = before_read;


                    if (ext_name.length() > 50 || dir_name.length() > 256 || file_name.length() > 256) {
                        std::cerr << "[WARNING] VPK: Skipping entry with suspiciously long names: "
                                                                 << "ext=" << ext_name.length()
                                                                 << ", dir=" << dir_name.length()
                                                                 << ", file=" << file_name.length() << std::endl;
                        continue;
                    }

                    bool valid = true;
                    for (char c : ext_name) {
                        if (static_cast<unsigned char>(c) < 32 || c > 126) { valid = false; break; }
                    }
                    for (char c : dir_name) {
                        if (static_cast<unsigned char>(c) < 32 || c > 126) { valid = false; break; }
                    }
                    for (char c : file_name) {
                        if (static_cast<unsigned char>(c) < 32 || c > 126) { valid = false; break; }
                    }

                    if (!valid) {
                        std::cerr << "[WARNING] VPK: Skipping entry with invalid characters (file: "
                                                                  << file_name << ")" << std::endl;
                        continue;
                    }

                    auto entry = std::make_shared<FileEntry>();
                    if (!entry) {
                        std::cerr << "[ERROR] VPK: Memory allocation failed" << std::endl;
                        return false;
                    }

                    entry->name = file_name + "." + ext_name;
                    entry->offset = entry_offset;
                    entry->size = entry_size;
                    entry->archive_index = archive_index;

                    if (dir_name != " " && !dir_name.empty()) {
                        entry->path = dir_name + "/" + file_name + "." + ext_name;
                    } else {
                        entry->path = file_name + "." + ext_name;
                    }

                    entry->is_directory = false;

                    if (!entry->name.empty() && !entry->path.empty()) {
                        root->files.push_back(entry);
                        file_count++;
                        file_count_local++;
                    }
                } else {
                    std::streampos err_pos = file.tellg();
                    std::cerr << "[ERROR] VPK: Invalid terminator 0x" << std::hex << term_flag
                                                              << std::dec << " expected 0xffff at offset " << (err_pos - static_cast<std::streamoff>(2))
                                                              << " (file: " << file_name << ")" << std::endl;
                    break;
                }
            }
        }
    }

    std::cout << "[INFO] VPK: Parsed " << file_count_local << " file entries from v2 archive" << std::endl;

    build_directory_structure(root);

    return file_count_local > 0;
}

void VpkParser::build_directory_structure(std::shared_ptr<DirectoryEntry>& root) {
    if (!root || root->files.empty()) return;

    std::ostringstream oss_build;
    oss_build << "VPK: Building directory hierarchy from " << root->files.size() << " files";
    LOG_DEBUG(oss_build.str());

    std::map<std::string, std::shared_ptr<DirectoryEntry>> dir_map;
    dir_map[""] = root;

    struct FileMoveInfo {
        std::shared_ptr<FileEntry> file;
        std::shared_ptr<DirectoryEntry> target_dir;
    };
    std::vector<FileMoveInfo> files_to_move;

    for (const auto& file : root->files) {
        if (!file) continue;

        size_t last_slash = file->path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string dir_path = file->path.substr(0, last_slash);
            std::string file_name_with_ext = file->path.substr(last_slash + 1);

            std::string current_path;
            std::shared_ptr<DirectoryEntry> current_parent = root;

            size_t pos = 0;
            while (pos < dir_path.length()) {
                size_t next_slash = dir_path.find('/', pos);
                if (next_slash == std::string::npos) {
                    next_slash = dir_path.length();
                }

                std::string dir_name = dir_path.substr(pos, next_slash - pos);
                if (!dir_name.empty() && dir_name != ".") {
                    if (current_path.empty()) {
                        current_path = dir_name;
                    } else {
                        current_path += "/" + dir_name;
                    }

                    if (dir_map.find(current_path) == dir_map.end()) {
                        auto new_dir = std::make_shared<DirectoryEntry>();
                        if (!new_dir) {
                            std::cerr << "[ERROR] VPK: Memory allocation failed for directory" << std::endl;
                            return;
                        }
                        new_dir->name = dir_name;
                        new_dir->is_directory = true;
                        new_dir->parent = current_parent;

                        current_parent->subdirectories.push_back(new_dir);
                        dir_map[current_path] = new_dir;
                    }

                    current_parent = dir_map[current_path];
                }

                pos = next_slash + 1;
            }

            file->name = file_name_with_ext;
            FileMoveInfo move_info;
            move_info.file = file;
            move_info.target_dir = current_parent;
            files_to_move.push_back(move_info);
        } else {
        }
    }

    for (const auto& move_info : files_to_move) {
        if (move_info.file && move_info.target_dir) {
            move_info.target_dir->files.push_back(move_info.file);
        }
    }

    root->files.erase(
        std::remove_if(root->files.begin(), root->files.end(),
            [&files_to_move](const std::shared_ptr<FileEntry>& f) {
                return std::any_of(files_to_move.begin(), files_to_move.end(),
                    [&f](const FileMoveInfo& move_info) {
                        return move_info.file == f;
                    });
            }),
        root->files.end()
    );

    std::ostringstream oss_dirs;
    oss_dirs << "VPK: Created " << (dir_map.size() - 1) << " directories";
    LOG_DEBUG(oss_dirs.str());
}

bool VpkParser::parse_vpk_dir(std::ifstream& file,
                                                              std::shared_ptr<DirectoryEntry>& root,
                                                              uint32_t& file_count) {
    std::cout << "[INFO] VPK: Parsing directory file format" << std::endl;

    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();

    file.seekg(4); // Skip 0x465456 signature

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);

    uint32_t tree_crc = 0;
    file.read(reinterpret_cast<char*>(&tree_crc), 4);

    uint32_t tree_size = 0;
    file.read(reinterpret_cast<char*>(&tree_size), 4);

    uint32_t file_crc = 0;
    file.read(reinterpret_cast<char*>(&file_crc), 4);

    uint32_t meta_crc = 0;
    file.read(reinterpret_cast<char*>(&meta_crc), 4);

    uint32_t content_crc = 0;
    file.read(reinterpret_cast<char*>(&content_crc), 4);

    std::cout << "[INFO] VPK: Dir Version=" << version << ", TreeCRC=0x" << std::hex << tree_crc
                              << std::dec << ", TreeSize=" << tree_size
                              << ", FileCRC=0x" << std::hex << file_crc
                              << std::dec << ", MetaCRC=0x" << std::hex << meta_crc
                              << std::dec << ", ContentCRC=0x" << std::hex << content_crc << std::dec << std::endl;

    uint32_t tree_offset = 28;

    if (tree_size == 0 || tree_size > static_cast<uint32_t>(file_size - tree_offset)) {
        std::cout << "[WARNING] VPK: Invalid tree_size in header (" << tree_size
                                  << "), scanning for valid tree data..." << std::endl;

        const uint64_t MAX_SCAN = 10000;
        bool found_valid_start = false;

        for (uint64_t scan = 0; scan < MAX_SCAN; scan += 4) {
            uint64_t pos = tree_offset + scan;
            if (pos >= file_size - 100) break;

            file.seekg(static_cast<std::streampos>(pos));

            std::string potential_ext = read_cstring(file);

            if (potential_ext.length() > 0 && potential_ext.length() <= 20) {
                bool all_printable = true;
                for (char c : potential_ext) {
                    if (c < 32 || c > 126) {
                        all_printable = false;
                        break;
                    }
                }

                if (all_printable) {
                    std::string potential_dir = read_cstring(file);

                    if (potential_dir.length() > 0 && potential_dir.length() <= 256) {
                        bool dir_valid = true;
                        for (char c : potential_dir) {
                            if (c < 32 || c > 126) {
                                dir_valid = false;
                                break;
                            }
                        }

                        if (dir_valid) {
                            std::cout << "[INFO] VPK: Found valid tree start at offset " << pos
                                                                         << " with extension '" << potential_ext << "'" << std::endl;
                            tree_offset = static_cast<uint32_t>(pos);
                            found_valid_start = true;
                            break;
                        }
                    }
                }
            }

            file.seekg(static_cast<std::streampos>(pos + 4));
        }

        if (!found_valid_start) {
            std::cout << "[WARNING] VPK: Could not find valid tree start, using default offset 12" << std::endl;
            tree_offset = 12;
        }
    } else {
        std::cout << "[INFO] VPK: Using tree_size from header: " << tree_size << " bytes" << std::endl;
    }

    file.seekg(tree_offset);

    DEBUG_COUT("[DEBUG] VPK Dir: Starting tree parsing at offset " << tree_offset
                              << ", file size: " << file_size << std::endl);

    std::streampos tree_end_pos;
    if (tree_size > 0 && tree_size < static_cast<uint32_t>(file_size)) {
        tree_end_pos = tree_offset + tree_size;
    } else {
        tree_end_pos = file_size - 48;
    }

    uint32_t file_count_local = 0;

    while (file.tellg() < tree_end_pos && file.good()) {
        std::string ext_name = read_cstring(file);

        if (ext_name.empty()) {
            uint16_t term_check = 0;
            file.read(reinterpret_cast<char*>(&term_check), 2);
            if (term_check == 0xffff) {
                break;
            }
            file.seekg(-2, std::ios_base::cur);
            continue;
        }

        DEBUG_COUT("[DEBUG] VPK: Dir - Extension: " << ext_name << std::endl);

        while (file.tellg() < tree_end_pos && file.good()) {
            std::string dir_name = read_cstring(file);

            if (dir_name.empty()) {
                break;
            }

            DEBUG_COUT("[DEBUG] VPK: Dir - Directory: " << dir_name << std::endl);

            while (file.tellg() < tree_end_pos && file.good()) {
                std::string file_name = read_cstring(file);

                if (file_name.empty()) {
                    break;
                }

                uint32_t crc_entry = 0;
                uint32_t preload_size = 0;
                uint32_t archive_index = 0;
                uint32_t entry_offset = 0;
                uint32_t entry_size = 0;

                file.read(reinterpret_cast<char*>(&crc_entry), 4);
                file.read(reinterpret_cast<char*>(&preload_size), 4);
                file.read(reinterpret_cast<char*>(&archive_index), 4);
                file.read(reinterpret_cast<char*>(&entry_offset), 4);
                file.read(reinterpret_cast<char*>(&entry_size), 4);

                if (!file.good()) {
                    break;
                }

                uint16_t term_flag = 0;
                file.read(reinterpret_cast<char*>(&term_flag), 2);

                if (term_flag == 0xffff) {
                    if (ext_name.length() > 50 || dir_name.length() > 512 || file_name.length() > 512) {
                        std::cerr << "[WARNING] VPK: Skipping entry with suspiciously long names: "
                                                                 << "ext=" << ext_name.length()
                                                                 << ", dir=" << dir_name.length()
                                                                 << ", file=" << file_name.length() << std::endl;
                        continue;
                    }

                    bool valid = true;
                    for (char c : ext_name) {
                        if (c < 32 || c > 126) { valid = false; break; }
                    }
                    for (char c : dir_name) {
                        if (c < 32 || c > 126) { valid = false; break; }
                    }
                    for (char c : file_name) {
                        if (c < 32 || c > 126) { valid = false; break; }
                    }

                    if (!valid) {
                        std::cerr << "[WARNING] VPK: Skipping entry with invalid characters" << std::endl;
                        continue;
                    }

                    auto entry = std::make_shared<FileEntry>();
                    if (!entry) {
                        std::cerr << "[ERROR] VPK: Memory allocation failed" << std::endl;
                        return false;
                    }

                    entry->name = file_name + "." + ext_name;
                    entry->offset = entry_offset;
                    entry->size = entry_size;
                    entry->archive_index = archive_index;

                    if (dir_name != " ") {
                        entry->path = dir_name + "/" + file_name + "." + ext_name;
                    } else {
                        entry->path = file_name + "." + ext_name;
                    }

                    DEBUG_COUT("[DEBUG] VPK: File=" << entry->path
                                                         << ", ArchiveIndex=" << archive_index
                                                         << ", Size=" << entry_size << std::endl);

                    entry->is_directory = false;

                    if (!entry->name.empty()) {
                        root->files.push_back(entry);
                        file_count++;
                        file_count_local++;
                    }
                } else {
                    break;
                }
            }
        }
    }

    std::cout << "[INFO] VPK: Parsed " << file_count_local << " file entries from directory file" << std::endl;
    return file_count_local > 0;
}

bool VpkParser::parse(const fs::path& archive_path,
                                         std::shared_ptr<DirectoryEntry>& root,
                                         uint32_t& file_count) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) {
        std::cerr << "[ERROR] VPK: Cannot open file: " << archive_path.string() << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();
    file.seekg(0);

    if (file_size < 8) {
        std::cerr << "[ERROR] VPK: File too small" << std::endl;
        return false;
    }

    uint32_t signature = 0;
    file.read(reinterpret_cast<char*>(&signature), 4);

    if (signature == 0x55aa1234) {
        return parse_vpk_v2(file, root, file_count);
    } else if (signature == 0x465456) {
        return parse_vpk_dir(file, root, file_count);
    } else {
        std::string filename = archive_path.filename().string();
        if (filename.find("_0") != std::string::npos ||
            filename.find("_1") != std::string::npos ||
            filename.find("_2") != std::string::npos) {
            std::cerr << "[ERROR] VPK: This appears to be a data-only VPK file (" << filename
                                              << "). Please load the main index file instead (_dir.vpk)" << std::endl;
        } else {
            std::cerr << "[ERROR] VPK: Invalid signature: 0x" << std::hex << signature << std::dec
                                              << " in file: " << filename << std::endl;
            std::cerr << "[INFO] Valid VPK files should have signature 0x55aa1234 or 0x465456" << std::endl;
        }
        return false;
    }
}

bool VpkParser::extract_file(const fs::path& archive_path,
                            const std::shared_ptr<FileEntry>& file,
                            std::vector<uint8_t>& data) const {
    if (!file) {
        std::cerr << "[ERROR] VPK: Invalid file entry" << std::endl;
        return false;
    }

    try {
        fs::path data_file_path = archive_path;

        if (file->archive_index == 0x7fff) {
            data_file_path = archive_path;
        } else {
            std::string dir_path = archive_path.string();
            std::string base_path = dir_path;

            size_t dir_pos = base_path.find("_dir.vpk");
            if (dir_pos != std::string::npos) {
                std::string archive_suffix = "_";
                if (file->archive_index < 10) {
                    archive_suffix += "00" + std::to_string(file->archive_index);
                } else if (file->archive_index < 100) {
                    archive_suffix += "0" + std::to_string(file->archive_index);
                } else {
                    archive_suffix += std::to_string(file->archive_index);
                }
                archive_suffix += ".vpk";
                base_path.replace(dir_pos, 8, archive_suffix);
                data_file_path = fs::path(base_path);
            } else {
                std::string stem = archive_path.stem().string();
                size_t dir_suffix_pos = stem.find("_dir");
                if (dir_suffix_pos != std::string::npos) {
                    stem.replace(dir_suffix_pos, 4, "");
                }

                std::string archive_suffix = "_";
                if (file->archive_index < 10) {
                    archive_suffix += "00" + std::to_string(file->archive_index);
                } else if (file->archive_index < 100) {
                    archive_suffix += "0" + std::to_string(file->archive_index);
                } else {
                    archive_suffix += std::to_string(file->archive_index);
                }

                data_file_path = archive_path.parent_path() / (stem + archive_suffix + ".vpk");
            }
        }

        if (read_from_data_file(data_file_path, file, data)) {
            return true;
        }

        std::cerr << "[WARNING] VPK: Direct data file open failed for "
                                  << data_file_path.string()
                                  << ", attempting fallback search across all VPK data archives in directory"
                                  << std::endl;

        if (fallback_search_data_archives(archive_path, file, data)) {
            return true;
        }

        std::cerr << "[ERROR] VPK: Failed to locate data for file: " << file->path << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] VPK: Exception in extract_file: " << e.what() << std::endl;
        return false;
    }
}

bool VpkParser::read_from_data_file(const fs::path& data_file_path,
                                    const std::shared_ptr<FileEntry>& file,
                                    std::vector<uint8_t>& data) const {
    try {
        std::ifstream ifs(data_file_path, std::ios::binary);
        if (!ifs.is_open()) {
            DEBUG_CERR("[DEBUG] VPK: Could not open data file: " << data_file_path.string() << std::endl);
            return false;
        }

        ifs.seekg(0, std::ios::end);
        std::streamoff total_size = ifs.tellg();
        if (file->offset >= static_cast<uint32_t>(total_size) ||
            static_cast<std::uint64_t>(file->offset) + static_cast<std::uint64_t>(file->size) > static_cast<std::uint64_t>(total_size)) {
            DEBUG_CERR("[DEBUG] VPK: Data range out of bounds in "
                                              << data_file_path.string()
                                              << " (offset=" << file->offset
                                              << ", size=" << file->size
                                              << ", total=" << total_size << ")" << std::endl);
            return false;
        }

        ifs.seekg(file->offset, std::ios::beg);
        if (!ifs.good()) {
            DEBUG_CERR("[DEBUG] VPK: Failed to seek to offset " << file->offset
                                              << " in " << data_file_path.string() << std::endl);
            return false;
        }

        data.resize(file->size);
        ifs.read(reinterpret_cast<char*>(data.data()), file->size);

        if (!ifs.good() && !ifs.eof()) {
            DEBUG_CERR("[DEBUG] VPK: Read error from data file: " << data_file_path.string() << std::endl);
            return false;
        }

        size_t bytes_read = static_cast<size_t>(ifs.gcount());
        if (bytes_read == 0) {
            DEBUG_CERR("[DEBUG] VPK: Zero bytes read from data file: " << data_file_path.string() << std::endl);
            return false;
        }

        if (bytes_read != file->size) {
            std::cerr << "[WARNING] VPK: Expected to read " << file->size
                                              << " bytes, but read " << bytes_read
                                              << " bytes from " << data_file_path.string() << std::endl;
            data.resize(bytes_read);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] VPK: Exception in read_from_data_file("
                                  << data_file_path.string() << "): " << e.what() << std::endl;
        return false;
    }
}

bool VpkParser::fallback_search_data_archives(const fs::path& archive_path,
                                                                                              const std::shared_ptr<FileEntry>& file,
                                                                                              std::vector<uint8_t>& data) const {
    try {
        fs::path dir = archive_path.parent_path();
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            DEBUG_CERR("[DEBUG] VPK: Parent directory does not exist or is not a directory: "
                                              << dir.string() << std::endl);
            return false;
        }

        std::string stem = archive_path.stem().string();
        std::string prefix = stem;
        const std::string dir_suffix = "_dir";
        auto pos = prefix.rfind(dir_suffix);
        if (pos != std::string::npos) {
            prefix.erase(pos, dir_suffix.length());
        }
        prefix += "_";

        bool any_tried = false;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            const auto& p = entry.path();
            if (!p.has_extension() || p.extension() != ".vpk") continue;

            std::string fname = p.filename().string();
            if (fname.find("_dir.vpk") != std::string::npos) continue;
            if (fname.rfind(prefix, 0) != 0) continue; // must start with prefix

            any_tried = true;
            DEBUG_CERR("[DEBUG] VPK: Fallback trying data archive: " << p.string() << std::endl);

            if (read_from_data_file(p, file, data)) {
                std::cerr << "[INFO] VPK: Fallback successfully read file data from "
                                                  << p.string() << std::endl;
                return true;
            }
        }

        if (!any_tried) {
            std::cerr << "[WARNING] VPK: No matching VPK data archives found for prefix '"
                                              << prefix << "' in directory " << dir.string() << std::endl;
        } else {
            std::cerr << "[WARNING] VPK: Fallback search did not find valid data for file: "
                                              << file->path << std::endl;
        }

        return false;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] VPK: Exception in fallback_search_data_archives: "
                                  << e.what() << std::endl;
        return false;
    }
}

} // namespace unpaker::parsers
