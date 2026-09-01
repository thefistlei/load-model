#include "learnopengl/scene_config.h"

#include <nlohmann_json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

static void loadTransform(const json& j, SceneTransform& t) {
    if (j.contains("position") && j["position"].is_array()) {
        auto arr = j["position"];
        for (int i = 0; i < 3 && i < (int)arr.size(); i++)
            t.position[i] = arr[i].get<float>();
    }
    if (j.contains("scaling") && j["scaling"].is_array()) {
        auto arr = j["scaling"];
        for (int i = 0; i < 3 && i < (int)arr.size(); i++)
            t.scaling[i] = arr[i].get<float>();
    }
    if (j.contains("euler_degrees") && j["euler_degrees"].is_array()) {
        auto arr = j["euler_degrees"];
        for (int i = 0; i < 3 && i < (int)arr.size(); i++)
            t.euler_degrees[i] = arr[i].get<float>();
    }
    if (j.contains("quaternion") && j["quaternion"].is_array()) {
        auto arr = j["quaternion"];
        for (int i = 0; i < 4 && i < (int)arr.size(); i++)
            t.quaternion[i] = arr[i].get<float>();
        t.has_quaternion = true;
    }
    if (j.contains("matrix") && j["matrix"].is_array()) {
        auto arr = j["matrix"];
        for (int i = 0; i < 16 && i < (int)arr.size(); i++)
            t.matrix[i] = arr[i].get<float>();
        t.has_matrix = true;
    }
}

static SceneMaterial loadMaterial(const json& j) {
    SceneMaterial m;
    if (j.contains("name"))
        m.name = j["name"].get<std::string>();
    if (j.contains("blend_mode"))
        m.blend_mode = j["blend_mode"].get<std::string>();
    if (j.contains("two_sided"))
        m.two_sided = j["two_sided"].get<bool>();
    if (j.contains("shader"))
        m.shader = j["shader"].get<std::string>();

    if (j.contains("textures") && j["textures"].is_object()) {
        for (auto& [key, val] : j["textures"].items()) {
            m.textures[key] = val.get<std::string>();
        }
    }
    if (j.contains("values") && j["values"].is_object()) {
        for (auto& [key, val] : j["values"].items()) {
            if (val.is_number())
                m.float_values[key] = val.get<float>();
        }
    }
    return m;
}

SceneConfig loadSceneConfig(const std::string& json_path) {
    SceneConfig config;

    std::ifstream file(json_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open scene config: " << json_path << std::endl;
        return config;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error in " << json_path << ": " << e.what() << std::endl;
        return config;
    }

    // base_dir is the directory containing scene.json
    auto pos = json_path.find_last_of("/\\");
    config.base_dir = (pos != std::string::npos) ? json_path.substr(0, pos) : ".";

    if (!root.contains("models") || !root["models"].is_array())
        return config;

    for (auto& mj : root["models"]) {
        SceneModel model;
        if (mj.contains("name"))
            model.name = mj["name"].get<std::string>();
        if (mj.contains("model_path"))
            model.model_path = mj["model_path"].get<std::string>();

        if (mj.contains("transform"))
            loadTransform(mj["transform"], model.transform);

        // scene.json also stores a precomputed matrix at the model root
        if (mj.contains("matrix") && mj["matrix"].is_array())
        {
            auto arr = mj["matrix"];
            for (int i = 0; i < 16 && i < (int)arr.size(); i++)
                model.transform.matrix[i] = arr[i].get<float>();
            model.transform.has_matrix = true;
        }

        if (mj.contains("materials") && mj["materials"].is_array()) {
            for (auto& matj : mj["materials"]) {
                model.materials.push_back(loadMaterial(matj));
            }
        }

        config.models.push_back(std::move(model));
    }

    return config;
}
