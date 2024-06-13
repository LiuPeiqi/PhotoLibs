#pragma once
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <vector>
#include "FileInfo.h"

class ExistPhotos {
public:
    template<class ValueType>
    using vector = std::vector<ValueType>;
    template<class KeyType, class ValueType>
    using multimap = std::multimap<KeyType, ValueType>;

    using file_clock         = std::chrono::file_clock;
    using file_time_type     = std::filesystem::file_time_type;
    using directory_iterator = std::filesystem::directory_iterator;
    using directory_entry    = std::filesystem::directory_entry;
    using path               = std::filesystem::path;

private:
    constexpr file_clock::duration one_year() {
        return std::chrono::duration_cast<file_clock::duration>(std::chrono::duration<int, std::chrono::year>(1));
    }

public:
    ExistPhotos(path root, bool recursive = true) {
        early = file_clock::now() + one_year(); // init a late time;
        for (const auto& entry : directory_iterator(root)) {
            if (entry.is_directory()) { remain_dirs.emplace_back(entry.path()); }
        }
        if (!recursive) { return; }
        size_t recursive_index = 0;
        while (recursive_index < remain_dirs.size()) {
            for (const auto& entry : directory_iterator(remain_dirs[recursive_index++])) {
                if (entry.is_directory()) { remain_dirs.emplace_back(entry.path()); }
            }
        }
    }

    template<class FilterType = bool(const directory_entry&)>
    size_t TraverseSceneTime(file_time_type scene, const FilterType& filter = [](const auto&) { return true; }) {
        if (scene > early) { return 0; }
        set<size_t> need_removed;
        size_t count = 0;
        for (size_t index = 0; index < remain_dirs.size(); ++index) {
            const auto& dir_path = remain_dirs[index];
            const auto dir_entry = directory_entry(dir_path);
            auto dir_time        = dir_entry.last_write_time();
            if (dir_time < scene) { continue; }
            need_removed.emplace(index);
            for (const auto& entry : directory_iterator(dir_path)) {
                if (entry.is_directory()) { continue; }
                auto last_time = entry.last_write_time();
                if (last_time < scene) { continue; }
                if (!filter(entry)) { continue; }
                auto file_path = entry.path();
                exist_files.emplace(file_path.filename(), {file_path, last_time, entry.file_size()});
                ++count;
            }
            if (early > dir_time) { early = dir_time; }
        }
        if (need_removed.size() > 0) {
            const auto& first = remain_dirs.front();
            size_t ri         = 0;
            std::erase_if(remain_dirs,
                          [&](const auto& v) { return need_removed.find(&v - &first) != need_removed.end(); });
        }
        return count;
    }

    vector<path> Difference(vector<FileInfo> sources) {
        vector<path> mismatch;
        for (const auto& file : sources) {
            const auto file_path = file.GetPath();
            auto [iter, end]     = exist_files.equal_range(file_path.filename());
            bool found           = false;
            for (; iter != end; ++iter) {
                if (iter->second.GetSize() == file.GetSize() && iter->second.GetWriteTime() == file.GetWriteTime()) {
                    found = true;
                    break;
                }
            }
            if (!found) { mismatch.emplace_back(file_path); }
        }
        return mismatch;
    }

protected:
    vector<path> remain_dirs;
    multimap<path, FileInfo> exist_files;
    file_time_type early;
};
