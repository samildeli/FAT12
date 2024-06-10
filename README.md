# FAT12

Basic FAT12-like file system and command line utilities.

## Usage

First, run make to compile makefs and fsutil.

Create a file system using makefs. Supported block sizes are 512, 1024, 2048, 4096.

```
makefs <fs_path> <block_size>
```

Operate on the file system using fsutil.

```
fsutil <fs_path> mkdir <dir_path>             Create a directory.
fsutil <fs_path> dir <dir_path>               List directory contents.
fsutil <fs_path> rmdir <dir_path>             Delete directory recursively.
fsutil <fs_path> write <dst_path> <src_path>  Copy external file to file in file system.
fsutil <fs_path> read <src_path> <dst_path>   Copy file in file system to external file.
fsutil <fs_path> del <file_path>              Delete file.
fsutil <fs_path> chmod <permissions> <path>   Change file or directory permissions.
fsutil <fs_path> dumpfs                       Print file system info and file tree.
```
