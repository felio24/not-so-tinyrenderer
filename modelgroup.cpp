#include <algorithm>
#include <utility>

#include "modelgroup.h"

std::vector<ModelGroup> discover_model_groups(const std::filesystem::path& obj_root) {
    namespace fs = std::filesystem;
    std::vector<ModelGroup> groups;

    if (!fs::is_directory(obj_root)) {
        return groups;
    }

    ModelGroup root_group{"(root)", {}};
    for (const fs::directory_entry& entry : fs::directory_iterator(obj_root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") {
            root_group.models.push_back(entry.path());
            continue;
        }

        if (!entry.is_directory()) {
            continue;
        }

        std::vector<fs::path> models = find_obj_files(entry.path());
        if (!models.empty()) {
            groups.push_back({entry.path().filename().string(), std::move(models)});
        }
    }

    std::sort(root_group.models.begin(), root_group.models.end());
    std::sort(groups.begin(), groups.end(), [](const ModelGroup& lhs, const ModelGroup& rhs) {
        return lhs.name < rhs.name;
    });

    if (!root_group.models.empty()) {
        groups.insert(groups.begin(), std::move(root_group));
    }

    return groups;
}

std::vector<std::filesystem::path> find_obj_files(
    const std::filesystem::path& input
) {
    namespace fs = std::filesystem;
    std::vector<fs::path> models;

    if (fs::is_regular_file(input) && input.extension() == ".obj") {
        models.push_back(input);
    } else if (fs::is_directory(input)) {
        for (const fs::directory_entry& entry :
             fs::recursive_directory_iterator(input)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".obj") {
                models.push_back(entry.path());
            }
        }
    }

    std::sort(models.begin(), models.end());
    return models;
}
