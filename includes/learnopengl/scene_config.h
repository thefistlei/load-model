#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct SceneMaterial {
    std::string name;
    std::string blend_mode;
    bool two_sided = false;
    std::unordered_map<std::string, std::string> textures; // name -> path
    std::unordered_map<std::string, float> float_values;   // name -> value
    std::string shader;
};

struct SceneTransform {
    float position[3] = {0, 0, 0};
    float scaling[3] = {1, 1, 1};
    float euler_degrees[3] = {0, 0, 0};
    float quaternion[4] = {0, 0, 0, 1};
    bool has_quaternion = false;
    float matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; // row-major 4x4
    bool has_matrix = false;
};

struct SceneModel {
    std::string name;
    std::string model_path; // relative to scene dir
    SceneTransform transform;
    std::vector<SceneMaterial> materials;
};

struct SceneConfig {
    std::vector<SceneModel> models;
    std::string base_dir; // directory of scene.json
};

// Parse scene.json, returns empty config on failure
SceneConfig loadSceneConfig(const std::string& json_path);
