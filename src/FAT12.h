#include "Disk.h"
#include <string>
#include <chrono>
#include <vector>
#include <array>
#include <cstring>
#include <filesystem>

class FAT12 {
public:
    using BlockAddress = int16_t;
    using Path = std::filesystem::path;

    struct FileAttributes {
        bool isDirectory;
        std::string name = "New File";
        BlockAddress size = 0;
        bool canRead = true;
        bool canWrite = true;
        int64_t created;
        int64_t lastModified;
    };

    FAT12(const std::string& diskPath, uint16_t blockSize);
    FAT12(const std::string& diskPath);

    void writeAttributes(const Path& path, const FileAttributes& attributes);
    FileAttributes readAttributes(const Path& path);

    void createDirectory(const Path& path);
    std::vector<FileAttributes> listDirectory(const Path& path);
    void deleteDirectory(const Path& path);

    void writeFile(const Path& path, const std::vector<char>& data);
    std::vector<char> readFile(const Path& path);
    void deleteFile(const Path& path);

    std::string dump();

private:
    static constexpr BlockAddress fatAddress() { return 0; }
    static constexpr BlockAddress freeBlockMarker() { return 0; }
    static constexpr BlockAddress lastBlockMarker() { return -1; }
    BlockAddress dataAddress() const { return fatAddress() + (fat.size() * sizeof(BlockAddress)) / sb.blockSize; }
    constexpr BlockAddress maxAddress() const { return fat.size() - 1; }
    int64_t getNow() const { return std::chrono::system_clock::now().time_since_epoch().count(); }

    struct Superblock {
        uint8_t partitionId = 1;
        uint16_t blockSize;
        BlockAddress rootDirectoryEntrySize = 0;
    };

    struct DirectoryEntry {
        FileAttributes attributes;
        BlockAddress firstBlockAddress = lastBlockMarker();
    };

    Disk disk;
    Superblock sb;
    std::array<BlockAddress, 4096> fat;

    std::string dumpDirectory(const Path& path, int indent, int& fileCount, int& directoryCount);

    void checkIsDirectory(const Path& path, bool shouldBeDirectory);
    void checkPermission(const Path& path, const std::string& permission);

    Path pathToName(const Path& path);
    Path parentPath(const Path& path);

    void writeSuperblock();
    void readSuperblock();
    void writeFat();
    void readFat();

    void writeBlock(BlockAddress blockAddress, const std::vector<char>& block);
    std::vector<char> readBlock(BlockAddress blockAddress);
    BlockAddress writeBlocks(BlockAddress blockAddress, const std::vector<char>& buffer);
    std::vector<char> readBlocks(BlockAddress blockAddress);

    void freeBlocks(const Path& path);
    void freeBlocks(BlockAddress blockAddress);

    void writeDirectoryEntry(const Path& path, const DirectoryEntry& directoryEntry);
    DirectoryEntry readDirectoryEntry(const Path& path);

    BlockAddress writeDirectory(const Path& path, const std::vector<DirectoryEntry>& directory, bool updateLastModified = false);
    std::vector<DirectoryEntry> readDirectory(const Path& path);

    std::pair<BlockAddress, BlockAddress> pathToAddressAndSize(const Path& path);
    std::vector<DirectoryEntry> readDirectory(BlockAddress blockAddress, BlockAddress size);

    template<typename T>
    void serialize(std::vector<char>& buffer, const T& data) const {
        static_assert(std::is_integral<T>::value);

        buffer.resize(buffer.size() + sizeof(T));
        std::memcpy(buffer.data() + buffer.size() - sizeof(T), &data, sizeof(T));
    }

    void serialize(std::vector<char>& buffer, const std::string& data) const {
        size_t length = data.size();
        buffer.resize(buffer.size() + sizeof(length));
        std::memcpy(buffer.data() + buffer.size() - sizeof(length), &length, sizeof(length));

        buffer.resize(buffer.size() + length);
        std::memcpy(buffer.data() + buffer.size() - length, data.data(), length);
    }

    template<typename T>
    void deserialize(const std::vector<char>& buffer, size_t& offset, T& data) const {
        static_assert(std::is_integral<T>::value);

        std::memcpy(&data, buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
    }
    
    void deserialize(const std::vector<char>& buffer, size_t& offset, std::string& data) const {
        size_t length;
        std::memcpy(&length, buffer.data() + offset, sizeof(length));
        offset += sizeof(length);

        data.resize(length);
        std::memcpy(data.data(), buffer.data() + offset, length);
        offset += length;
    }
};
