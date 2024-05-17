#include <proxy/proxy.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs     = std::filesystem;
namespace Holder = std::placeholders;

using Time   = fs::file_time_type;
using Path   = fs::path;
using String = std::string;
using Char   = char;

class FileInfo {
public:
    FileInfo(const Path& path, const Time& time, size_t size): path(path), last_write_time(time), file_size(size) {}

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

    const Path& GetPath() const noexcept { return path; }
    const Path GetFileName() const noexcept { return path.filename(); }
    const size_t GetSize() const noexcept { return file_size; }
    const Time& GetWriteTime() const noexcept { return last_write_time; }


protected:
    Path path;
    Path category;
    String md5;
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

size_t Traverse(const fs::path& path, std::function<void(const Path&, const Time&, size_t)> InsertFile,
                const std::function<bool(const fs::directory_entry&)>& filter) {
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (!filter(entry)) { continue; }
        if (entry.is_directory()) {
            count += Traverse(entry.path(), InsertFile, filter);
        } else if (entry.is_regular_file()) {
            InsertFile(entry.path(), entry.last_write_time(), entry.file_size());
            ++count;
        }
    }
    return count;
}

int main(int argc, Char** argv) {
    if (argc < 2) { return -1; }
    Path source(argv[1]);
    Path destination(argv[2]);

    std::vector<FileInfo> src_files;
    std::map<Path, FileInfo> des_files;
    src_files.reserve(256);
    auto filter_all = [](const auto&) { return true; };

    Traverse(source,
             std::bind(&std::vector<FileInfo>::emplace_back<const Path&, const Time&, size_t>, &src_files, Holder::_1,
                       Holder::_2, Holder::_3),
             filter_all);
    Traverse(
        destination,
        [&des_files](const Path& path, const Time& time, size_t size) {
            des_files.emplace(std::make_pair(path.filename(), FileInfo{path, time, size}));
        },
        filter_all);
    //auto Order      = [](const FileInfo& left, const FileInfo& right) { return left < right; };
    size_t count    = 0;
    auto end_of_des = des_files.end();
    for (const auto& file : src_files) {
        auto iter = des_files.find(file.GetFileName());
        if (iter == end_of_des) {
            std::cout << "Missing:" << file.GetPath() << std::endl;
            continue;
        }
        if (iter->second.GetSize() == file.GetSize() && iter->second.GetWriteTime() == file.GetWriteTime()) {
            ++count;
        } else {
            std::cout << std::format("Meta mismatch: {} vs {}\n", file.GetPath().string(),
                                     iter->second.GetPath().string());
        }
    }
    std::cout << "Find count:" << count;
    return 0;
}
