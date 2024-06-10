#include "FAT12.h"
#include "exceptions.h"
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cassert>

using Path = std::filesystem::path;

void errorExit(const char* usage) {
    std::cerr << "Invalid arguments. Usage: fsutil <fs_path> " << usage << std::endl;
    std::exit(1);
}

std::string timeToString(int64_t time) {
    time /= 1000000000; // Convert nanoseconds to seconds.
    std::tm* t = std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(t, "%FT%TZ");
    return oss.str();
}

int getDigitCount(int size) {
    assert(size >= 0);
    if (size == 0) {
        return 1;
    }
    return std::log10(size) + 1;
}

void dir(FAT12& fs, const Path& path) {
    auto list = fs.listDirectory(path);

    // Pad sizes to max digit count.
    int maxDigitCount = 1;
    for (auto& attributes : list) {
        int digitCount = getDigitCount(attributes.size);
        if (digitCount > maxDigitCount) {
            maxDigitCount = digitCount;
        }
    }

    for (auto& attributes : list) {
        std::cout << (attributes.isDirectory ? "d" : "-");
        std::cout << (attributes.canRead ? "r" : "-");
        std::cout << (attributes.canWrite ? "w" : "-") << " ";
        std::cout << std::setw(10) << timeToString(attributes.created) << " ";
        std::cout << std::setw(10) << timeToString(attributes.lastModified) << " ";
        std::cout << std::setw(maxDigitCount) << attributes.size << " ";
        std::cout << attributes.name;
        std::cout << std::endl;
    }
}

void write(FAT12& fs, const Path& dstPath, const Path& srcPath) {
    // Read external source file.
    std::ifstream file(srcPath, std::ios::binary | std::ios::ate);
    assert(file);
    auto size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);

    // Write to destination file in file system.
    fs.writeFile(dstPath, buffer);

    // Copy permissions.
    auto permissions = std::filesystem::status(srcPath).permissions();
    auto attributes = fs.readAttributes(dstPath);
    attributes.canRead = (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none;
    attributes.canWrite = (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
    fs.writeAttributes(dstPath, attributes);
}

void read(FAT12& fs, const Path& srcPath, const Path& dstPath) {
    // Read source file from file system.
    auto buffer = fs.readFile(srcPath);

    // Write to external destination file.
    std::ofstream file(dstPath, std::ios::binary);
    assert(file);
    file.write(buffer.data(), buffer.size());

    // Copy permissions.
    auto attributes = fs.readAttributes(srcPath);
    auto permissions = std::filesystem::perms::none;
    if (attributes.canRead) {
        permissions |= std::filesystem::perms::owner_read;
    }
    if (attributes.canWrite) {
        permissions |= std::filesystem::perms::owner_write;
    }
    std::filesystem::permissions(dstPath, permissions);
}

void chmod(FAT12& fs, const Path& path, const std::string& permissions) {
    auto attributes = fs.readAttributes(path);

    if (permissions[0] != '+' && permissions[0] != '-') {
        throw InvalidModeException(path);
    }

    bool add;
    for (auto c : permissions) {
        if (c == '+') {
            add = true;
        } else if (c == '-') {
            add = false;
        } else if (c == 'r') {
            attributes.canRead = add;
        } else if (c == 'w') {
            attributes.canWrite = add;
        } else {
            throw InvalidModeException(path);
        }
    }

    fs.writeAttributes(path, attributes);
}

std::string normalizePath(std::string path) {
    // Convert backslashes to forward slashes.
    std::replace(path.begin(), path.end(), '\\', '/');

    // Remove trailing slash.
    if (path.length() > 1 && path[path.length() - 1] == '/') {
        path.pop_back();
    }

    return path;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Not enough arguments." << std::endl;
        return 1;
    }

    FAT12 fs(argv[1]);

    try {
        if (std::string(argv[2]) == "mkdir") {
            if (argc < 4) {
                errorExit("mkdir <dir_path>");
            }
        
            fs.createDirectory(normalizePath(argv[3]));
        }
        else if (std::string(argv[2]) == "dir") {
            if (argc < 4) {
                errorExit("dir <dir_path>");
            }

            dir(fs, normalizePath(argv[3]));
        }
        else if (std::string(argv[2]) == "rmdir") {
            if (argc < 4) {
                errorExit("rmdir <dir_path>");
            }

            fs.deleteDirectory(normalizePath(argv[3]));
        }
        else if (std::string(argv[2]) == "write") {
            if (argc < 5) {
                errorExit("write <dst_path> <src_path>");
            }

            write(fs, normalizePath(argv[3]), normalizePath(argv[4]));
        }
        else if (std::string(argv[2]) == "read") {
            if (argc < 5) {
                errorExit("read <src_path> <dst_path>");
            }
            
            read(fs, normalizePath(argv[3]), normalizePath(argv[4]));
        }
        else if (std::string(argv[2]) == "del") {
            if (argc < 4) {
                errorExit("del <file_path>");
            }

            fs.deleteFile(normalizePath(argv[3]));
        }
        else if (std::string(argv[2]) == "chmod") {
            if (argc < 5) {
                errorExit("chmod <permissions> <path>");
            }

            chmod(fs, normalizePath(argv[4]), argv[3]);
        }
        else if (std::string(argv[2]) == "dumpfs") {
            std::cout << fs.dump();
        }
        else {
            std::cerr << "Invalid subcommand." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
