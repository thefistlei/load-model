#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <learnopengl/app_engine.h>
#include <learnopengl/platform.h>

#include <iostream>
#include <cstdio>

#ifndef SS_GL_USE_ES
#define SS_GL_USE_ES 0
#endif

static const unsigned int SCR_WIDTH = 800;
static const unsigned int SCR_HEIGHT = 600;

static GLFWwindow* gWindow = nullptr;

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

static void processInput()
{
    if (!gWindow)
        return;

    if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(gWindow, true);

    Camera& camera = appGetCamera();
    const float dt = appGetDeltaTime();

    if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, dt);
}

static GLFWwindow* createGLFWWindow(int width, int height, const char* title)
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
    platformInitAssets();

    const std::string modelArg = resolveLaunchModelArg(argc, argv);
    if (modelArg.empty())
    {
        std::cerr << "Usage:\n";
        std::cerr << "  1) Put config.json next to the exe:\n";
        std::cerr << "       { \"model\": \"fy\" }\n";
        std::cerr << "     then run: " << argv[0] << "\n";
        std::cerr << "  2) Or pass CLI (overrides config): " << argv[0] << " <model_path>\n";
        return 1;
    }

    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    gWindow = createGLFWWindow(SCR_WIDTH, SCR_HEIGHT, "Model Loading");
    if (gWindow == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);

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
#endif

    AppCallbacks callbacks;
    callbacks.width = SCR_WIDTH;
    callbacks.height = SCR_HEIGHT;
    callbacks.shouldClose = []() { return glfwWindowShouldClose(gWindow) != 0; };
    callbacks.processInput = processInput;
    callbacks.getTime = []() { return glfwGetTime(); };
    callbacks.swapBuffers = []() { glfwSwapBuffers(gWindow); };
    callbacks.pollEvents = []() { glfwPollEvents(); };

    const int result = runModelLoadingApp(modelArg, callbacks);

    glfwTerminate();
    return result;
}
