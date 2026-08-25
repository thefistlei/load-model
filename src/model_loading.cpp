#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>

#ifndef SS_GL_USE_ES
#define SS_GL_USE_ES 0
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <climits>
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
        std::cerr << "Usage: " << argv[0] << " <model_path>\n";
        std::cerr << "Example: " << argv[0] << " backpack/backpack.obj\n";
        return 1;
    }

    const std::string modelPath = joinPath(getExecutableDirectory(), argv[1]);
    if (!fileExists(modelPath))
    {
        std::cerr << "Model file not found: " << modelPath << std::endl;
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
    Model ourModel(modelPath);

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

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        ourShader.setMat4("model", model);
        ourModel.Draw(ourShader);

        frameCount++;
        double now = glfwGetTime();
        if (now - fpsTimer >= 1.0)
        {
            char title[64];
            std::snprintf(title, sizeof(title), "Model Loading - FPS: %d", frameCount);
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
