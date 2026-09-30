#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ModelGroup {
    std::string name;
    std::vector<std::filesystem::path> models;
};

std::vector<ModelGroup> discover_model_groups(const std::filesystem::path& obj_root);
std::vector<std::filesystem::path> find_obj_files(const std::filesystem::path& input);