#pragma once

#include <filesystem>

namespace kh
{
struct DataFolder
{
    std::string folder_path;
    std::vector<std::string> filenames;
};

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

std::optional<DataFolder> find_data_folder(gsl::span<const std::string> folder_paths)
{
    for (const auto& folder_path : folder_paths) {
        auto filenames{get_filenames_from_folder_path(folder_path)};
        if (filenames)
            return DataFolder{folder_path, *filenames};
    }

    return std::nullopt;
}
}