#include <array>
#include <fstream>

class Disk {
public:
    Disk(const std::string& path, bool trunc);

    static constexpr int sectorSize = 512;

    void write(size_t address, const std::array<char, sectorSize>& sector);
    std::array<char, sectorSize> read(size_t address);
private:
    std::fstream fs;
};
