#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace filesystem = std::filesystem;

void traverse(const std::string& path) {
    for (const auto& entry : filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            std::error_code ec;
            auto space_info = std::filesystem::space(entry.path(), ec);
            std::cout << std::format("\nDir:{}, Capacity:{}, Free:{}, Available:{}\n", entry.path().string(),
                                     space_info.capacity, space_info.free, space_info.available);
            traverse(entry.path().string());
        } else if (entry.is_regular_file()) {
            auto status = entry.status();
            std::cout << std::format(
                "Name:{}, Ext:{}, Size:{}, LastWriteTime:{}, CanRead:{}\n", entry.path().filename().string(),
                entry.path().extension().string(), entry.file_size(), entry.last_write_time(),
                static_cast<unsigned int>(status.permissions()
                                          & (std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                                             | std::filesystem::perms::others_read)));
        }
    }
}

int main(int argc, char** argv) {
    traverse(".");
    return 0;
}
