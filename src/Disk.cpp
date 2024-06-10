#include "Disk.h"
#include <iostream>
#include <cassert>

Disk::Disk(const std::string& path, bool trunc) :
    fs(path, trunc ? std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc
                   : std::ios::binary | std::ios::in | std::ios::out
    )
{
    assert(fs);
}

void Disk::write(size_t address, const std::array<char, sectorSize>& sector) {
    fs.seekg(address * sectorSize);
    fs.write(sector.data(), sectorSize);
}

std::array<char, Disk::sectorSize> Disk::read(size_t address) {
    std::array<char, sectorSize> buffer;
    
    fs.seekg(address * sectorSize);
    fs.read(buffer.data(), sectorSize);

    return buffer;
}
