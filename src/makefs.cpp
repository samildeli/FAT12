#include "FAT12.h"
#include <iostream>

void errorExit() {
    std::cerr << "Invalid arguments. Usage: makefs <fs_path> <block_size(512|1024|2048|4096)>" << std::endl;
    std::exit(1);
}

int main(int argc, char* argv[]) {
    std::string blockSize = argv[2];
    if (argc < 3 || blockSize != "512" && blockSize != "1024" && blockSize != "2048" && blockSize != "4096") {
        errorExit();
    }

    FAT12 fat12(argv[1], std::atoi(blockSize.c_str()));

    return 0;
}
