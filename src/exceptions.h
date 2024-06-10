#include <stdexcept>

class FileSystemException : public std::runtime_error {
public:
    FileSystemException(const std::string& path, const std::string& message) :
        std::runtime_error(path + ": " + message) {}
};

class NoSuchFileOrDirectoryException : public FileSystemException {
public:
    NoSuchFileOrDirectoryException(const std::string& path) :
        FileSystemException(path, "No such file or directory.") {}
};

class NotADirectoryException : public FileSystemException {
public:
    NotADirectoryException(const std::string& path) :
        FileSystemException(path, "Not a directory.") {}
};

class IsADirectoryException : public FileSystemException {
public:
    IsADirectoryException(const std::string& path) :
        FileSystemException(path, "Is a directory.") {}
};

class FileExistsException : public FileSystemException {
public:
    FileExistsException(const std::string& path) :
        FileSystemException(path, "Cannot create directory: File exists.") {}
};

class PermissionException : public FileSystemException {
public:
    PermissionException(const std::string& path) :
        FileSystemException(path, "Permission denied.") {}
};

class InvalidModeException : public FileSystemException {
public:
    InvalidModeException(const std::string& path) :
        FileSystemException(path, "Invalid mode.") {}
};
