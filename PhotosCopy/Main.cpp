#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace filesystem = std::filesystem;

void traverse(const std::string& path) {
    for (const auto& entry : filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            std::cout << "Directory: " << entry.path().string() << std::endl;
            traverse(entry.path().string());
        } else if (entry.is_regular_file()) {
            std::cout << "File: " << entry.path().string() << std::endl;
            std::cout << "Name: " << entry.path().filename().string() << std::endl;
            std::cout << "Ext:  " << entry.path().extension().string() << std::endl;
            std::cout << "Path: " << entry.path().parent_path().string() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    traverse(".");
    return 0;
}
