#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/scene_config.h>
#include <learnopengl/platform.h>
#include <learnopengl/app_engine.h>
#include <nlohmann_json.hpp>
#include <stb_image.h>

#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <sstream>

#ifndef SS_GL_USE_ES
#define SS_GL_USE_ES 0
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "model_loading", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "model_loading", __VA_ARGS__)
#else
#define LOGI(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOGE(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#endif

static Camera gCamera(glm::vec3(0.0f, 0.0f, 3.0f));
static float gDeltaTime = 0.0f;
static float gLastFrame = 0.0f;

static std::string joinPath(const std::string& dir, const std::string& rel)
{
    if (dir.empty() || dir == ".")
        return rel;
    const char last = dir.back();
    if (last == '/' || last == '\\')
        return dir + rel;
    return dir + "/" + rel;
}

static bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string basenameOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

static std::string loadModelArgFromConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
        return "";

    if (endsWith(configPath, ".json"))
    {
        try
        {
            nlohmann::json root;
            file >> root;
            if (root.contains("model") && root["model"].is_string())
                return root["model"].get<std::string>();
            if (root.contains("model_path") && root["model_path"].is_string())
                return root["model_path"].get<std::string>();
        }
        catch (const std::exception& e)
        {
            LOGE("Failed to parse config %s: %s", configPath.c_str(), e.what());
        }
        return "";
    }

    std::string line;
    while (std::getline(file, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            ++start;
        if (start > 0)
            line = line.substr(start);
        if (line.empty() || line[0] == '#')
            continue;
        return line;
    }
    return "";
}

std::string resolveLaunchModelArg(int argc, char* argv[])
{
    if (argc >= 2 && argv[1] && argv[1][0] != '\0')
        return argv[1];

    const std::string dataRoot = platformDataRoot();
    const std::string candidates[] = {
        joinPath(dataRoot, "config.json"),
        joinPath(dataRoot, "model_loading.json"),
        "config.json",
    };

    for (const auto& path : candidates)
    {
        if (!platformFileExists(path))
            continue;
        std::string model = loadModelArgFromConfig(path);
        if (!model.empty())
        {
            LOGI("Config: %s -> model=\"%s\"", path.c_str(), model.c_str());
            return model;
        }
        LOGE("Config %s has no \"model\" / \"model_path\"", path.c_str());
    }

    return "";
}

static std::string resolveModelPath(const std::string& arg)
{
    const std::string base = joinPath(platformDataRoot(), arg);
    if (!platformIsDirectory(base))
        return base;

    std::string sceneJson = joinPath(base, "scene.json");
    if (platformFileExists(sceneJson))
        return sceneJson;

    const std::string name = basenameOf(arg);
    const std::string candidates[] = {
        joinPath(base, name + ".obj"),
        joinPath(base, name + ".fbx"),
        joinPath(base, "fy.obj"),
    };

    for (const auto& candidate : candidates)
    {
        if (platformFileExists(candidate))
            return candidate;
    }

    return base;
}

static glm::mat4 transformToMat4(const SceneTransform& t)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(t.position[0], t.position[1], t.position[2]));

    if (t.has_quaternion)
    {
        glm::quat q(t.quaternion[0], t.quaternion[1], t.quaternion[2], t.quaternion[3]);
        model *= glm::mat4_cast(q);
    }
    else
    {
        model = glm::rotate(model, glm::radians(t.euler_degrees[0]), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(t.euler_degrees[1]), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(t.euler_degrees[2]), glm::vec3(0, 0, 1));
    }

    model = glm::scale(model, glm::vec3(t.scaling[0], t.scaling[1], t.scaling[2]));
    return model;
}

static void frameCameraToScene(const SceneConfig& config)
{
    glm::vec3 bmin(1e9f), bmax(-1e9f);
    bool any = false;
    for (const auto& sm : config.models)
    {
        glm::vec3 p(sm.transform.position[0], sm.transform.position[1], sm.transform.position[2]);
        bmin = glm::min(bmin, p);
        bmax = glm::max(bmax, p);
        any = true;
    }
    if (!any)
        return;

    glm::vec3 center = 0.5f * (bmin + bmax);
    float radius = glm::max(glm::length(bmax - bmin) * 0.5f, 10.0f);

    gCamera.Position = center + glm::vec3(0.0f, radius * 0.6f, radius * 1.8f);
    gCamera.Yaw = -90.0f;
    gCamera.Pitch = -25.0f;
    gCamera.MovementSpeed = glm::max(radius * 0.4f, 10.0f);
    gCamera.ProcessMouseMovement(0.0f, 0.0f, false);

    LOGI("Camera framed at (%.1f, %.1f, %.1f) radius=%.1f",
         gCamera.Position.x, gCamera.Position.y, gCamera.Position.z, radius);
}

Camera& appGetCamera()
{
    return gCamera;
}

float appGetDeltaTime()
{
    return gDeltaTime;
}

void appSetDeltaTime(float dt)
{
    gDeltaTime = dt;
}

int runModelLoadingApp(const std::string& modelArg, AppCallbacks& callbacks)
{
    if (modelArg.empty())
        return 1;

    const std::string resolvedPath = resolveModelPath(modelArg);
    const bool isSceneConfig = endsWith(resolvedPath, "scene.json");

    if (!platformFileExists(resolvedPath))
    {
        LOGE("Model file not found: %s", resolvedPath.c_str());
        return 1;
    }

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    const std::string dataRoot = platformDataRoot();
#if SS_GL_USE_ES
    Shader ourShader(
        joinPath(dataRoot, "1.model_loading_es.vs").c_str(),
        joinPath(dataRoot, "1.model_loading_es.fs").c_str());
#else
    Shader ourShader(
        joinPath(dataRoot, "1.model_loading.vs").c_str(),
        joinPath(dataRoot, "1.model_loading.fs").c_str());
#endif

    struct LoadedModel {
        std::unique_ptr<Model> model;
        glm::mat4 transform;
        std::string name;
    };

    std::vector<LoadedModel> models;

    if (isSceneConfig)
    {
        LOGI("Loading scene config: %s", resolvedPath.c_str());
        SceneConfig config = loadSceneConfig(resolvedPath);
        if (config.models.empty())
        {
            LOGE("No models in scene config");
            return 1;
        }

        for (auto& sm : config.models)
        {
            std::string fullPath = joinPath(config.base_dir, sm.model_path);
            if (!platformFileExists(fullPath))
            {
                LOGE("skip %s: %s not found", sm.name.c_str(), fullPath.c_str());
                continue;
            }

            LOGI("loading: %s <- %s", sm.name.c_str(), sm.model_path.c_str());

            LoadedModel lm;
            lm.model = std::make_unique<Model>(fullPath);
            lm.transform = transformToMat4(sm.transform);
            lm.name = sm.name;

            bool gotTex = false;
            for (const auto& mat : sm.materials)
            {
                auto it = mat.textures.find("BaseMap");
                if (it == mat.textures.end() || it->second.empty())
                    continue;
                lm.model->EnsureDiffuseTexture(it->second, config.base_dir);
                gotTex = true;
                break;
            }
            if (!gotTex)
                lm.model->EnsureFallbackDiffuse();

            models.push_back(std::move(lm));
        }

        if (models.empty())
        {
            LOGE("No models loaded successfully");
            return 1;
        }

        frameCameraToScene(config);
        glDisable(GL_CULL_FACE);
    }
    else
    {
        LoadedModel lm;
        lm.model = std::make_unique<Model>(resolvedPath);
        lm.transform = glm::mat4(1.0f);
        lm.name = basenameOf(resolvedPath);
        lm.model->EnsureFallbackDiffuse();
        models.push_back(std::move(lm));
    }

    int frameCount = 0;
    double fpsTimer = callbacks.getTime ? callbacks.getTime() : 0.0;

    while (true)
    {
        if (callbacks.shouldClose && callbacks.shouldClose())
            break;
        const float currentFrame = static_cast<float>(callbacks.getTime ? callbacks.getTime() : 0.0);
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        if (callbacks.processInput)
            callbacks.processInput();

        const int width = callbacks.width > 0 ? callbacks.width : 800;
        const int height = callbacks.height > 0 ? callbacks.height : 600;

        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        glm::mat4 projection = glm::perspective(
            glm::radians(gCamera.Zoom),
            static_cast<float>(width) / static_cast<float>(height),
            0.1f, 5000.0f);
        glm::mat4 view = gCamera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        for (auto& lm : models)
        {
            ourShader.setMat4("model", lm.transform);
            lm.model->Draw(ourShader);
        }

        frameCount++;
        const double now = callbacks.getTime ? callbacks.getTime() : 0.0;
        if (now - fpsTimer >= 1.0)
        {
            LOGI("FPS: %d models=%zu fps=%d", (int)models.size(), models.size(), frameCount);
            frameCount = 0;
            fpsTimer = now;
        }

        if (callbacks.swapBuffers)
            callbacks.swapBuffers();
        if (callbacks.pollEvents)
            callbacks.pollEvents();
    }

    return 0;
}
