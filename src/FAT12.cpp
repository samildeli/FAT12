#include "FAT12.h"
#include "exceptions.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <algorithm>

FAT12::FAT12(const std::string& diskPath, uint16_t blockSize) :
    disk(diskPath, true),
    sb({.blockSize = blockSize})
{
    assert(blockSize == 512 || blockSize == 1024 || blockSize == 2048 || blockSize == 4096);

    fat.fill(freeBlockMarker());
    
    // Occupy addresses for FAT in FAT.
    for (int i = 0; i < dataAddress(); i++) {
        fat[i] = lastBlockMarker();
    }

    // Write the root directory entry to dataAddress().
    fat[dataAddress()] = lastBlockMarker();
    auto now = getNow();
    DirectoryEntry rootDirectoryEntry = {
        .attributes = {
            .isDirectory = true,
            .name = "/",
            .created = now,
            .lastModified = now
        }
    };
    // Empty string represents the directory that contains root directory entry.
    sb.rootDirectoryEntrySize = writeDirectory("", {rootDirectoryEntry});

    writeSuperblock();
    writeFat();
}

FAT12::FAT12(const std::string& diskPath) :
    disk(diskPath, false)
{
    readSuperblock();
    readFat();
}

void FAT12::writeAttributes(const Path& path, const FileAttributes& attributes) {
    DirectoryEntry entry = readDirectoryEntry(path);
    entry.attributes = attributes;
    writeDirectoryEntry(path, entry);
}

FAT12::FileAttributes FAT12::readAttributes(const Path& path) {
    DirectoryEntry entry = readDirectoryEntry(path);
    return entry.attributes;
}

void FAT12::createDirectory(const Path& path) {
    checkPermission(parentPath(path), "w");

    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    // Check if it already exists.
    for (auto& entry : parent) {
        if (entry.attributes.name == name) {
            throw FileExistsException(path);
        }
    }

    // Add new directory's directory entry to its parent directory.
    auto now = getNow();
    DirectoryEntry entry = {
        .attributes = {
            .isDirectory = true,
            .name = name,
            .created = now,
            .lastModified = now
        }
    };
    parent.push_back(entry);
    writeDirectory(parentPath(path), parent, true);
}

std::vector<FAT12::FileAttributes> FAT12::listDirectory(const Path& path) {
    checkPermission(path, "r");

    // If it is a file, return its attributes.
    auto attributes = readAttributes(path);
    if (!attributes.isDirectory) {
        return {attributes};
    }

    auto directory = readDirectory(path);
    std::vector<FileAttributes> list;

    for (auto& entry : directory) {
        list.push_back(entry.attributes);
    }

    return list;
}

void FAT12::deleteDirectory(const Path& path) {
    checkPermission(path, "w");

    auto self = readDirectory(path);
    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    // Delete directory recursively.
    for (auto& entry : self) {
        if (entry.attributes.isDirectory) {
            deleteDirectory(path/entry.attributes.name);
        } else {
            freeBlocks(entry.firstBlockAddress);
        }
    }

    // Free the blocks occupied by directory.
    freeBlocks(path);

    for (size_t i = 0; i < parent.size(); i++) {
        if (parent[i].attributes.name == name) {
            // Remove directory's directory entry from its parent directory.
            parent.erase(parent.begin() + i);
            writeDirectory(parentPath(path), parent, true);
            return;
        }
    }
}

void FAT12::writeFile(const Path& path, const std::vector<char>& data) {
    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    try {
        // Try writing to existing file.
        checkIsDirectory(path, false);
        checkPermission(path, "w");

        auto [address, size] = pathToAddressAndSize(path);
        writeBlocks(address, data);

        // Update attributes.
        auto attributes = readAttributes(path);
        attributes.size = data.size();
        attributes.lastModified = getNow();
        writeAttributes(path, attributes);
    } catch (const NoSuchFileOrDirectoryException& e) {
        // Create new file.
        checkPermission(parentPath(path), "w");

        auto address = writeBlocks(lastBlockMarker(), data);

        // Add new file's directory entry to its parent directory.
        auto now = getNow();
        DirectoryEntry entry = {
            .attributes = {
                .isDirectory = false,
                .name = name,
                .size = (BlockAddress)data.size(),
                .created = now,
                .lastModified = now
            },
            .firstBlockAddress = address
        };
        parent.push_back(entry);
        writeDirectory(parentPath(path), parent, true);
    }
}

std::vector<char> FAT12::readFile(const Path& path) {
    checkIsDirectory(path, false);
    checkPermission(path, "r");

    auto [address, size] = pathToAddressAndSize(path);

    auto data = readBlocks(address);
    data.resize(size);

    return data;
}

void FAT12::deleteFile(const Path& path) {
    checkIsDirectory(path, false);
    checkPermission(path, "w");

    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    // Free the blocks occupied by file.
    freeBlocks(path);

    for (size_t i = 0; i < parent.size(); i++) {
        if (parent[i].attributes.name == name) {
            // Remove file's directory entry from its parent directory.
            parent.erase(parent.begin() + i);
            writeDirectory(parentPath(path), parent, true);
            return;
        }
    }
}

std::string FAT12::dump() {
    std::ostringstream oss;
    
    int freeBlockCount = 0;
    for (BlockAddress address : fat) {
        if (address == freeBlockMarker()) {
            freeBlockCount++;
        }
    }

    int fileCount = 0;
    int directoryCount = 0;
    std::string directoryDump = dumpDirectory("", 0, fileCount, directoryCount);

    oss << "Block count: " << fat.size() << std::endl;
    oss << "Free blocks: " << freeBlockCount << std::endl;
    oss << "Block size: " << sb.blockSize << std::endl;
    oss << "File count: " << fileCount << std::endl;
    oss << "Directory count: " << directoryCount << std::endl;
    oss << directoryDump;

    return oss.str();
}

std::string FAT12::dumpDirectory(const Path& path, int indent, int& fileCount, int& directoryCount) {
    std::ostringstream oss;
    auto directory = readDirectory(path);

    for (auto& entry : directory) {
        oss << std::string(indent, ' ');
        oss << entry.attributes.name << " ";
        
        // Write contiguous addresses with a dash between begin and end addresses.
        // Write "->" to denote jumping to a noncontiguous address.
        BlockAddress beginAddress = entry.firstBlockAddress;
        if (beginAddress != -1) {
            oss << beginAddress;
        }
        for (BlockAddress address = entry.firstBlockAddress; address != lastBlockMarker(); address = fat[address]) {
            if (fat[address] != address + 1) {
                if (address != beginAddress) {
                    oss << "-" << address;
                }
                if (fat[address] != lastBlockMarker()) {
                    oss << "->" << fat[address];
                    beginAddress = fat[address];
                }
            }
        }

        oss << std::endl;

        if (entry.attributes.isDirectory) {
            directoryCount++;
            oss << dumpDirectory(path/entry.attributes.name, indent + 2, fileCount, directoryCount);
        } else {
            fileCount++;
        }
    }

    return oss.str();
}

void FAT12::checkIsDirectory(const Path& path, bool shouldBeDirectory) {
    bool isDirectory = path.empty() || readAttributes(path).isDirectory;
    if (shouldBeDirectory && !isDirectory) {
        throw NotADirectoryException(path);
    }
    if (!shouldBeDirectory && isDirectory) {
        throw IsADirectoryException(path);
    }
}

void FAT12::checkPermission(const Path& path, const std::string& permission) {
    assert(permission == "r" || permission == "w");

    // The directory that contains root directory entry has no attributes.
    if (path.empty()) {
        return;
    }

    auto attributes = readAttributes(path);
    if (permission == "r" && !attributes.canRead || permission == "w" && !attributes.canWrite) {
        throw PermissionException(path);
    }
}

FAT12::Path FAT12::pathToName(const Path& path) {
    assert(!path.empty());

    if (path == path.root_path()) {
        return "/";
    }

    if (path.has_filename()) {
        return path.filename();
    }

    return path.parent_path().filename();
}

FAT12::Path FAT12::parentPath(const Path& path) {
    assert(!path.empty());

    if (path == path.root_path()) {
        return "";
    }

    return path.parent_path();
}

void FAT12::writeSuperblock() {
    std::vector<char> buffer;
    serialize(buffer, sb.partitionId);
    serialize(buffer, sb.blockSize);
    serialize(buffer, sb.rootDirectoryEntrySize);

    // Write superblock to sector 0.
    std::array<char, Disk::sectorSize> sector;
    std::copy_n(buffer.begin(), buffer.size(), sector.begin());
    disk.write(0, sector);
}

void FAT12::readSuperblock() {
    // Read superblock from sector 0.
    auto sector = disk.read(0);

    std::vector<char> buffer(sector.begin(), sector.end());
    size_t offset = 0;
    deserialize(buffer, offset, sb.partitionId);
    deserialize(buffer, offset, sb.blockSize);
    deserialize(buffer, offset, sb.rootDirectoryEntrySize);
}

void FAT12::writeFat() {
    std::vector<char> buffer;

    for (BlockAddress blockAddress : fat) {
        serialize(buffer, blockAddress);
    }

    for (BlockAddress blockAddress = fatAddress(); blockAddress < dataAddress(); blockAddress++) {
        auto begin = buffer.begin() + blockAddress * sb.blockSize;
        std::vector<char> block(begin, begin + sb.blockSize);
        writeBlock(blockAddress, block);
    }
}

void FAT12::readFat() {
    std::vector<char> buffer;
    size_t offset = 0;

    for (BlockAddress blockAddress = fatAddress(); blockAddress < dataAddress(); blockAddress++) {
        std::vector<char> block = readBlock(blockAddress);
        buffer.insert(buffer.end(), block.begin(), block.end());
    }
    
    for (BlockAddress& blockAddress : fat) {
        deserialize(buffer, offset, blockAddress);
    }
}

void FAT12::writeBlock(BlockAddress blockAddress, const std::vector<char>& block) {
    assert(blockAddress >= 0 && blockAddress <= maxAddress());
    assert(block.size() == sb.blockSize);

    size_t sectorPerBlock = sb.blockSize / Disk::sectorSize;
    size_t startAddress = 1 + blockAddress * sectorPerBlock; // Blocks start at sector 1, after superblock.

    for (size_t offset = 0; offset < sectorPerBlock; offset++) {
        std::array<char, Disk::sectorSize> sector;
        std::copy_n(block.begin() + offset * Disk::sectorSize, Disk::sectorSize, sector.begin());
        disk.write(startAddress + offset, sector);
    }
}

std::vector<char> FAT12::readBlock(BlockAddress blockAddress) {
    assert(blockAddress >= 0 && blockAddress <= maxAddress());

    std::vector<char> block(sb.blockSize);

    size_t sectorPerBlock = sb.blockSize / Disk::sectorSize;
    size_t startAddress = 1 + blockAddress * sectorPerBlock; // Blocks start at sector 1, after superblock.
    
    for (size_t offset = 0; offset < sectorPerBlock; offset++) {
        auto sector = disk.read(startAddress + offset);
        std::copy_n(sector.begin(), Disk::sectorSize, block.begin() + offset * Disk::sectorSize);
    }

    return block;
}

FAT12::BlockAddress FAT12::writeBlocks(BlockAddress blockAddress, const std::vector<char>& buffer) {
    if (blockAddress == lastBlockMarker()) {
        // No existing blocks, start checking for free blocks from the beginning of data blocks.
        blockAddress = dataAddress();
    } else {
        // Free existing blocks from the specified address so that we can write to it.
        freeBlocks(blockAddress);
    }

    if (buffer.empty()) {
        return lastBlockMarker();
    }

    BlockAddress currAddress = blockAddress;
    BlockAddress prevAddress = -1;
    BlockAddress firstAddress = -1;
    size_t offset = 0;
    while (true) {
        if (fat[currAddress] == freeBlockMarker()) {
            // Free block was found, write next block in buffer to it.
            auto begin = buffer.begin() + offset;
            std::vector<char> block(begin, std::min(begin + sb.blockSize, buffer.end()));
            block.resize(sb.blockSize);
            offset += sb.blockSize;
            writeBlock(currAddress, block);

            // Save firstBlockAddress.
            if (firstAddress == -1) {
                firstAddress = currAddress;
            }

            // Form link between previous block and current block in FAT.
            if (prevAddress != -1) {
                fat[prevAddress] = currAddress;
            }
            prevAddress = currAddress;

            // If everything in buffer is written, save fat and return.
            if (offset >= buffer.size()) {
                fat[currAddress] = lastBlockMarker();
                writeFat();
                return firstAddress;
            }
        }

        // Go to next block and check if we looped to where we began.
        currAddress = (currAddress + 1) % fat.size();
        if (currAddress == blockAddress) {
            throw std::runtime_error("File system is full.");
        }
    }
}

std::vector<char> FAT12::readBlocks(BlockAddress blockAddress) {
    assert(blockAddress >= dataAddress() && blockAddress <= maxAddress() || blockAddress == lastBlockMarker());

    std::vector<char> buffer;

    while (blockAddress != lastBlockMarker()) {
        auto block = readBlock(blockAddress);
        buffer.insert(buffer.end(), block.begin(), block.end());
        blockAddress = fat[blockAddress];
    }

    return buffer;
}

void FAT12::freeBlocks(const Path& path) {
    auto [address, _] = pathToAddressAndSize(path);
    freeBlocks(address);
}

void FAT12::freeBlocks(BlockAddress blockAddress) {
    assert(blockAddress >= dataAddress() && blockAddress <= maxAddress() || blockAddress == lastBlockMarker());

    while (blockAddress != lastBlockMarker()) {
        BlockAddress nextAddress = fat[blockAddress];
        fat[blockAddress] = freeBlockMarker();
        blockAddress = nextAddress;
    }

    writeFat();
}

void FAT12::writeDirectoryEntry(const Path& path, const DirectoryEntry& directoryEntry) {
    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    for (auto& entry : parent) {
        if (entry.attributes.name == name) {
            entry = directoryEntry;
            writeDirectory(parentPath(path), parent);
            return;
        }
    }

    throw NoSuchFileOrDirectoryException(path);
}

FAT12::DirectoryEntry FAT12::readDirectoryEntry(const Path& path) {
    auto parent = readDirectory(parentPath(path));
    std::string name = pathToName(path);

    for (auto& entry : parent) {
        if (entry.attributes.name == name) {
            return entry;
        }
    }

    throw NoSuchFileOrDirectoryException(path);
}

FAT12::BlockAddress FAT12::writeDirectory(const Path& path, const std::vector<DirectoryEntry>& directory, bool updateLastModified) {
    checkIsDirectory(path, true);

    std::vector<char> buffer;

    for (auto& entry : directory) {
        serialize(buffer, entry.attributes.isDirectory);
        serialize(buffer, entry.attributes.name);
        serialize(buffer, entry.attributes.size);
        serialize(buffer, entry.attributes.canRead);
        serialize(buffer, entry.attributes.canWrite);
        serialize(buffer, entry.attributes.created);
        serialize(buffer, entry.attributes.lastModified);
        serialize(buffer, entry.firstBlockAddress);
    }

    auto [address, _] = pathToAddressAndSize(path);
    address = writeBlocks(address, buffer);

    // Update directory's directory entry.
    if (path.empty()) {
        sb.rootDirectoryEntrySize = buffer.size();
        writeSuperblock();
    } else {
        auto entry = readDirectoryEntry(path);
        bool updated = false;

        if (entry.attributes.size != buffer.size() || entry.firstBlockAddress != address) {
            entry.attributes.size = buffer.size();
            entry.firstBlockAddress = address;
            updated = true;
        }

        if (updateLastModified) {
            entry.attributes.lastModified = getNow();
            updated = true;
        }

        if (updated) {
            writeDirectoryEntry(path, entry);
        }
    }

    return buffer.size();
}

std::vector<FAT12::DirectoryEntry> FAT12::readDirectory(const Path& path) {
    checkIsDirectory(path, true);
    auto [address, size] = pathToAddressAndSize(path);
    return readDirectory(address, size);
}

std::pair<FAT12::BlockAddress, FAT12::BlockAddress> FAT12::pathToAddressAndSize(const Path& path) {
    // Start with the address and size of the directory that contains root directory entry.
    auto address = dataAddress();
    auto size = sb.rootDirectoryEntrySize;

    // If this is the directory that contains root directory entry, return with the initial address and size.
    if (path.empty()) {
        return {address, size};
    }

    auto directory = readDirectory(address, size);
    bool fileFound = false;
    Path currPath = "";

    for (auto& name : path) {
        bool found = false;

        if (fileFound) {
            throw NotADirectoryException(currPath);
        }

        currPath = currPath/name;

        for (auto& entry : directory) {
            if (entry.attributes.name == name) {
                found = true;
                address = entry.firstBlockAddress;
                size = entry.attributes.size;

                if (entry.attributes.isDirectory) {
                    directory = readDirectory(address, size);
                } else {
                    fileFound = true;
                }

                break;
            }
        }

        if (!found) {
            throw NoSuchFileOrDirectoryException(currPath);
        }
    }

    return {address, size};
}

std::vector<FAT12::DirectoryEntry> FAT12::readDirectory(BlockAddress blockAddress, BlockAddress size) {
    std::vector<DirectoryEntry> directory;
    auto buffer = readBlocks(blockAddress);
    size_t offset = 0;

    while (offset < size) {
        DirectoryEntry entry;

        deserialize(buffer, offset, entry.attributes.isDirectory);
        deserialize(buffer, offset, entry.attributes.name);
        deserialize(buffer, offset, entry.attributes.size);
        deserialize(buffer, offset, entry.attributes.canRead);
        deserialize(buffer, offset, entry.attributes.canWrite);
        deserialize(buffer, offset, entry.attributes.created);
        deserialize(buffer, offset, entry.attributes.lastModified);
        deserialize(buffer, offset, entry.firstBlockAddress);

        directory.push_back(entry);
    }
    
    return directory;
}
