#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using Time   = fs::file_time_type;

class FileInfo {
public:
    FileInfo(const fs::path& path, const Time& time, size_t size): path(path), last_write_time(time), file_size(size) {}

    bool Equal(const FileInfo& other) const noexcept {
        if (!md5.empty() && !other.md5.empty()) { return md5 == other.md5; }
        return path == other.path && category == other.category && last_write_time == other.last_write_time
               && file_size == other.file_size;
    }

    int Compare(const FileInfo& other) const noexcept {
        if (Equal(other)) { return 0; }
        auto cr = path.compare(other.path);
        if (cr != 0) { return cr; }
        cr = category.compare(other.category);
        if (cr != 0) { return cr; }
        if (last_write_time.time_since_epoch() < other.last_write_time.time_since_epoch()) { return -1; }
        if (file_size < other.file_size) { return -1; }
        if (file_size == other.file_size) { return 0; }
        return 1;
    }

    const fs::path& GetPath() const noexcept { return path; }
    const fs::path GetFileName() const noexcept { return path.filename(); }
    const size_t GetSize() const noexcept { return file_size; }
    const Time& GetWriteTime() const noexcept { return last_write_time; }


protected:
    fs::path path;
    fs::path category;
    std::string md5;
    Time last_write_time;
    size_t file_size;
};

bool operator==(const FileInfo& left, const FileInfo& right) noexcept {
    return left.Equal(right);
}

bool operator<(const FileInfo& left, const FileInfo& right) noexcept {
    return left.Compare(right) < 0;
}

bool operator<=(const FileInfo& left, const FileInfo& right) noexcept {
    return left.Compare(right) <= 0;
}

bool operator>(const FileInfo& left, const FileInfo& right) noexcept {
    return left.Compare(right) > 0;
}

bool operator>=(const FileInfo& left, const FileInfo& right) noexcept {
    return left.Compare(right) >= 0;
}
