#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/scene_config.h>

#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>
#include <memory>
#include <vector>

#ifndef SS_GL_USE_ES
#define SS_GL_USE_ES 0
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <climits>
#include <sys/stat.h>
#endif

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

std::string getExecutableDirectory()
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return ".";
    std::string path(buffer, len);
    const size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0)
        return ".";
    buffer[len] = '\0';
    std::string path(buffer);
    const size_t pos = path.find_last_of('/');
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#endif
}

std::string joinPath(const std::string& dir, const std::string& rel)
{
    if (dir.empty() || dir == ".")
        return rel;
    const char last = dir.back();
    if (last == '/' || last == '\\')
        return dir + rel;
    return dir + "/" + rel;
}

bool fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

bool isDirectory(const std::string& path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

std::string basenameOf(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Resolve model path: if arg is a directory, look for scene.json, then fallback to .obj/.fbx
std::string resolveModelPath(const std::string& arg)
{
    const std::string base = joinPath(getExecutableDirectory(), arg);
    if (!isDirectory(base))
        return base;

    // Check for scene.json first
    std::string sceneJson = joinPath(base, "scene.json");
    if (fileExists(sceneJson))
        return sceneJson;

    const std::string name = basenameOf(arg);
    const std::string candidates[] = {
        joinPath(base, name + ".obj"),
        joinPath(base, name + ".fbx"),
        joinPath(base, "fy.obj"),
    };

    for (const auto& candidate : candidates)
    {
        if (fileExists(candidate))
            return candidate;
    }

    return base;
}

// Convert SceneTransform to glm::mat4
glm::mat4 transformToMat4(const SceneTransform& t)
{
    if (t.has_matrix)
    {
        // matrix is row-major float[16], glm expects column-major
        return glm::make_mat4(t.matrix);
    }

    glm::mat4 model = glm::mat4(1.0f);

    // Apply translation
    model = glm::translate(model, glm::vec3(t.position[0], t.position[1], t.position[2]));

    // Apply rotation
    if (t.has_quaternion)
    {
        // quaternion [x, y, z, w] -> glm::quat(w, x, y, z)
        glm::quat q(t.quaternion[3], t.quaternion[0], t.quaternion[1], t.quaternion[2]);
        model *= glm::mat4_cast(q);
    }
    else
    {
        // Euler angles in degrees
        model = glm::rotate(model, glm::radians(t.euler_degrees[0]), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(t.euler_degrees[1]), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(t.euler_degrees[2]), glm::vec3(0, 0, 1));
    }

    // Apply scaling
    model = glm::scale(model, glm::vec3(t.scaling[0], t.scaling[1], t.scaling[2]));

    return model;
}

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

GLFWwindow* createGLFWWindow(int width, int height, const char* title)
{
#if SS_GL_USE_ES
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);

    const int versions[][2] = {{3, 2}, {0, 0}, {3, 1}, {3, 0}};
    GLFWwindow* window = nullptr;

    for (const auto& ver : versions)
    {
        if (ver[0] == 0 && ver[1] == 0)
            continue;

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_ALPHA_BITS, 0);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ver[0]);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ver[1]);

        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window)
            break;
    }

    return window;
#else
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#if defined(__linux__)
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    return glfwCreateWindow(width, height, title, nullptr, nullptr);
#endif
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <model_path_or_scene_dir>\n";
        std::cerr << "Example: " << argv[0] << " backpack/backpack.obj\n";
        std::cerr << "         " << argv[0] << " fy   (loads fy/scene.json or fy/fy.obj)\n";
        return 1;
    }

    const std::string resolvedPath = resolveModelPath(argv[1]);

    // Check if this is a scene.json
    bool isSceneConfig = (resolvedPath.size() >= 11 &&
        resolvedPath.substr(resolvedPath.size() - 11) == "scene.json");

    if (!fileExists(resolvedPath))
    {
        std::cerr << "Model file not found: " << resolvedPath << std::endl;
        if (isDirectory(joinPath(getExecutableDirectory(), argv[1])))
            std::cerr << "Hint: expected scene.json, " << argv[1] << ".obj or fy.obj" << std::endl;
        return 1;
    }

    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    GLFWwindow* window = createGLFWWindow(SCR_WIDTH, SCR_HEIGHT, "Model Loading");
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

#if !SS_GL_USE_ES
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
#else
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize OpenGL ES" << std::endl;
        return -1;
    }
    std::cout << "OpenGL ES " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
#endif

    stbi_set_flip_vertically_on_load(true);

    glEnable(GL_DEPTH_TEST);

#if SS_GL_USE_ES
    Shader ourShader("1.model_loading_es.vs", "1.model_loading_es.fs");
#else
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");
#endif

    // Scene mode: load multiple models with transforms
    struct LoadedModel {
        std::unique_ptr<Model> model;
        glm::mat4 transform;
        std::string name;
    };

    std::vector<LoadedModel> models;

    if (isSceneConfig)
    {
        std::cout << "Loading scene config: " << resolvedPath << std::endl;
        SceneConfig config = loadSceneConfig(resolvedPath);
        if (config.models.empty())
        {
            std::cerr << "No models in scene config" << std::endl;
            glfwTerminate();
            return 1;
        }

        std::cout << "Scene has " << config.models.size() << " models" << std::endl;

        for (auto& sm : config.models)
        {
            std::string fullPath = joinPath(config.base_dir, sm.model_path);
            if (!fileExists(fullPath))
            {
                std::cerr << "  skip " << sm.name << ": " << fullPath << " not found" << std::endl;
                continue;
            }

            std::cout << "  loading: " << sm.name << " <- " << sm.model_path << std::endl;

            LoadedModel lm;
            lm.model = std::make_unique<Model>(fullPath);
            lm.transform = transformToMat4(sm.transform);
            lm.name = sm.name;
            models.push_back(std::move(lm));
        }

        if (models.empty())
        {
            std::cerr << "No models loaded successfully" << std::endl;
            glfwTerminate();
            return 1;
        }

        std::cout << "Loaded " << models.size() << " models" << std::endl;

        // Adjust camera to be further back for large scenes
        camera.ProcessKeyboard(BACKWARD, 50.0f);
    }
    else
    {
        // Single model mode (backward compat)
        LoadedModel lm;
        lm.model = std::make_unique<Model>(resolvedPath);
        lm.transform = glm::mat4(1.0f);
        lm.name = basenameOf(resolvedPath);
        models.push_back(std::move(lm));
    }

    int frameCount = 0;
    double fpsTimer = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        for (auto& lm : models)
        {
            ourShader.setMat4("model", lm.transform);
            lm.model->Draw(ourShader);
        }

        frameCount++;
        double now = glfwGetTime();
        if (now - fpsTimer >= 1.0)
        {
            char title[128];
            std::snprintf(title, sizeof(title), "Model Loading - %d models - FPS: %d",
                          (int)models.size(), frameCount);
            glfwSetWindowTitle(window, title);
            frameCount = 0;
            fpsTimer = now;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
