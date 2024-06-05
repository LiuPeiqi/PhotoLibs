#include <proxy/proxy.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs     = std::filesystem;
namespace Holder = std::placeholders;

using Time = fs::file_time_type;

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

size_t Traverse(const fs::path& path, std::function<void(const fs::path&, const Time&, size_t)> InsertFile,
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

bool IsPhoto(const fs::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](auto c) { return std::tolower(c); });
    for (auto& know : {".jpg", ".jpeg", ".arw"}) {
        if (know == ext) { return true; }
    }
    return false;
}

std::vector<fs::path> GuessPhotosDir(const fs::path& root, std::function<bool(const fs::path&)> filter = IsPhoto) {
    std::set<fs::path> dirs_set;
    std::vector<fs::path> dirs;
    std::function<void(const fs::path&)> traverse = [&](const fs::path& dir_path) {
        bool skip = false;
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_directory()) {
                traverse(entry.path());
            } else if (skip) {
                continue;
            } else if (entry.is_regular_file() && filter(entry.path())) {
                const auto& abs_path = fs::absolute(dir_path);
                if (dirs_set.find(abs_path) == dirs_set.end()) {
                    dirs_set.emplace(abs_path);
                    dirs.emplace_back(abs_path);
                    skip = true;
                }
            }
        }
    };
    traverse(root);
    return dirs;
}

std::vector<std::vector<fs::path>> PhotoGroups(const std::vector<fs::path>& dirs) {
    std::vector<std::pair<Time, fs::path>> timestamp_dirs;
    std::transform(dirs.begin(), dirs.end(), std::back_inserter(timestamp_dirs), [](const fs::path& path) {
        return std::make_pair(fs::directory_entry(path).last_write_time(), path);
    });
    std::sort(timestamp_dirs.begin(), timestamp_dirs.end(), [](const auto& left, const auto& right) {
        return left.first.time_since_epoch() < right.first.time_since_epoch();
    });
    auto last_time = Time().time_since_epoch();
    std::vector<std::vector<fs::path>> dir_groups;
    for (const auto& dir_pair : timestamp_dirs) {
        const auto& timestamp = dir_pair.first.time_since_epoch();
        const auto& path      = dir_pair.second;
        const auto expired    = std::chrono::duration_cast<std::chrono::hours>(timestamp - last_time).count();
        if (expired > 39) {
            last_time = timestamp;
            dir_groups.emplace_back(std::vector<fs::path>());
        }
        dir_groups.back().emplace_back(path);
    }
    return dir_groups;
}

void DisplayGroup(const std::vector<std::filesystem::path>& group, int group_id, std::string status) {
    auto p0 = std::chrono::clock_cast<std::chrono::system_clock>(fs::last_write_time(group.front()));
    auto p1 = std::chrono::clock_cast<std::chrono::system_clock>(fs::last_write_time(group.back()));
    std::cout << std::format("group {} since: [{:%F}] to: [{:%F}] | {}", ++group_id, p0, p1, status) << std::endl;
    for (const auto& path : group) {
        std::cout << std::format("\t\t {}", path.string()) << std::endl;
    }
    std::cout << std::endl << std::endl;
}

void DisplayGroups(const std::vector<std::vector<fs::path>>& groups) {
    int group_id = 0;
    for (const auto group : groups) {
        DisplayGroup(group, group_id, "");
    }
}

std::tuple<size_t, size_t> DirDifference(const std::vector<FileInfo>& src_files,
                                         const std::map<fs::path, FileInfo>& des_files) {
    size_t missing  = 0;
    size_t mismatch = 0;
    auto end_of_des = des_files.end();
    for (const auto& file : src_files) {
        auto iter = des_files.find(file.GetFileName());
        if (iter == end_of_des) {
            //std::cout << "Missing:" << file.GetPath() << std::endl;
            ++missing;
            continue;
        }
        if (iter->second.GetSize() == file.GetSize() && iter->second.GetWriteTime() == file.GetWriteTime()) {
            // same
        } else {
            std::cout << std::format("Meta mismatch: {} vs {}\n", file.GetPath().string(),
                                     iter->second.GetPath().string());
            ++mismatch;
        }
    }
    return {missing, mismatch};
}

int main(int argc, char** argv) {
    auto dirs   = GuessPhotosDir(argv[1]);
    auto groups = PhotoGroups(dirs);
    if (argc < 2) {
        DisplayGroups(groups);
        return 0;
    }

    fs::path destination(argv[2]);
    std::map<fs::path, FileInfo> des_files;
    Traverse(
        destination,
        [&des_files](const fs::path& path, const Time& time, size_t size) {
            des_files.emplace(std::make_pair(path.filename(), FileInfo{path, time, size}));
        },
        IsPhoto);

    int group_id = 0;
    for (const auto& group : groups) {
        std::vector<FileInfo> src_files;
        src_files.reserve(256);
        for (const auto& source : group) {
            Traverse(source,
                     std::bind(&std::vector<FileInfo>::emplace_back<const fs::path&, const Time&, size_t>, &src_files,
                               Holder::_1, Holder::_2, Holder::_3),
                     IsPhoto);
        }
        auto [missing, mismatch] = DirDifference(src_files, des_files);
        std::string status =
            (missing == mismatch && missing == 0) ? "OK" : std::format("missing:{}, mismatch:{}", missing, mismatch);
        DisplayGroup(group, ++group_id, status);
    }

    return 0;
}
