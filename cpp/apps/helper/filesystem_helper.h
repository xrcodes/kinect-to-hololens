#pragma once

#include <filesystem>

namespace kh
{
std::vector<std::string> get_filenames_from_folder_path(std::string folder_path)
{
    std::vector<std::string> filenames;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
            std::string filename = entry.path().filename().string();
            if (filename == ".gitignore")
                continue;
            if (entry.is_directory())
                continue;
            filenames.push_back(filename);
        }
    } catch (std::filesystem::filesystem_error e) {
        std::cout << "Error finding files at " << folder_path << " from get_filenames_from_folder_path():\n  " << e.what() << "\n";
        return std::vector<std::string>();
    }

    return filenames;
}
}