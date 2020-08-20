#pragma once

#include <filesystem>

namespace kh
{
std::optional<std::vector<std::string>> get_filenames_from_folder_path(std::string folder_path)
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
        return std::nullopt;
    }

    return filenames;
}
}