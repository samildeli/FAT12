CXX = g++
CXXFLAGS = -std=c++20 -pedantic

all: makefs fsutil

makefs: src/makefs.cpp src/FAT12.cpp src/FAT12.h src/Disk.cpp src/Disk.h src/exceptions.h
	$(CXX) $(CXXFLAGS) -o makefs src/makefs.cpp src/FAT12.cpp src/Disk.cpp

fsutil: src/fsutil.cpp src/FAT12.cpp src/FAT12.h src/Disk.cpp src/Disk.h src/exceptions.h
	$(CXX) $(CXXFLAGS) -o fsutil src/fsutil.cpp src/FAT12.cpp src/Disk.cpp

.PHONY: clean
clean:
	rm makefs fsutil
