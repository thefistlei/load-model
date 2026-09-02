#pragma once

#include <learnopengl/camera.h>

#include <functional>
#include <string>

struct AppCallbacks {
    int width = 800;
    int height = 600;
    std::function<bool()> shouldClose;
    std::function<void()> processInput;
    std::function<void(int, int)> onResize;
    std::function<double()> getTime;
    std::function<void()> swapBuffers;
    std::function<void()> pollEvents;
};

std::string resolveLaunchModelArg(int argc, char* argv[]);
int runModelLoadingApp(const std::string& modelArg, AppCallbacks& callbacks);

Camera& appGetCamera();
float appGetDeltaTime();
void appSetDeltaTime(float dt);
