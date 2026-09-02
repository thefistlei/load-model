#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <glad/glad.h>

#include <learnopengl/app_engine.h>
#include <learnopengl/platform.h>
#include <learnopengl/platform_android.h>

#include <string>
#include <chrono>

#define LOG_TAG "model_loading"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static android_app* gApp = nullptr;
static EGLDisplay gDisplay = EGL_NO_DISPLAY;
static EGLSurface gSurface = EGL_NO_SURFACE;
static EGLContext gContext = EGL_NO_CONTEXT;
static ANativeWindow* gWindow = nullptr;
static int gWidth = 0;
static int gHeight = 0;
static bool gEglReady = false;
static bool gRunning = true;

static bool gTouchActive = false;
static float gLastTouchX = 0.0f;
static float gLastTouchY = 0.0f;

static double nowSeconds()
{
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

static void pollAndroidEvents()
{
    if (!gApp)
        return;

    int events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0)
    {
        if (source != nullptr)
            source->process(gApp, source);
        if (gApp->destroyRequested != 0)
            gRunning = false;
    }
}

static bool initEgl(ANativeWindow* window)
{
    gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (gDisplay == EGL_NO_DISPLAY)
        return false;

    if (!eglInitialize(gDisplay, nullptr, nullptr))
        return false;

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(gDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
        return false;

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    gSurface = eglCreateWindowSurface(gDisplay, config, window, nullptr);
    gContext = eglCreateContext(gDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (gSurface == EGL_NO_SURFACE || gContext == EGL_NO_CONTEXT)
        return false;

    if (!eglMakeCurrent(gDisplay, gSurface, gSurface, gContext))
        return false;

    gWidth = ANativeWindow_getWidth(window);
    gHeight = ANativeWindow_getHeight(window);
    glViewport(0, 0, gWidth, gHeight);
    return true;
}

static void terminateEgl()
{
    if (gDisplay == EGL_NO_DISPLAY)
        return;

    eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (gContext != EGL_NO_CONTEXT)
        eglDestroyContext(gDisplay, gContext);
    if (gSurface != EGL_NO_SURFACE)
        eglDestroySurface(gDisplay, gSurface);
    eglTerminate(gDisplay);

    gContext = EGL_NO_CONTEXT;
    gSurface = EGL_NO_SURFACE;
    gDisplay = EGL_NO_DISPLAY;
    gEglReady = false;
}

static void handleAppCmd(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
        if (app->window != nullptr && !gEglReady)
        {
            gWindow = app->window;
            if (!initEgl(gWindow))
            {
                LOGE("EGL init failed");
                gRunning = false;
            }
            else
            {
                gEglReady = true;
                LOGI("EGL ready %dx%d", gWidth, gHeight);
            }
        }
        break;
    case APP_CMD_TERM_WINDOW:
        terminateEgl();
        gWindow = nullptr;
        break;
    case APP_CMD_DESTROY:
        gRunning = false;
        break;
    default:
        break;
    }
}

static int32_t handleInput(android_app* app, AInputEvent* event)
{
    (void)app;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION)
        return 0;

    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const float x = AMotionEvent_getX(event, 0);
    const float y = AMotionEvent_getY(event, 0);
    Camera& camera = appGetCamera();

    switch (action)
    {
    case AMOTION_EVENT_ACTION_DOWN:
        gTouchActive = true;
        gLastTouchX = x;
        gLastTouchY = y;
        return 1;
    case AMOTION_EVENT_ACTION_MOVE:
        if (gTouchActive)
        {
            const float dx = x - gLastTouchX;
            const float dy = gLastTouchY - y;
            camera.ProcessMouseMovement(dx, dy);
            gLastTouchX = x;
            gLastTouchY = y;
        }
        return 1;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
        gTouchActive = false;
        return 1;
    default:
        break;
    }
    return 0;
}

void android_main(android_app* app)
{
    gApp = app;
    app->onAppCmd = handleAppCmd;
    app->onInputEvent = handleInput;

    bool appStarted = false;

    while (gRunning)
    {
        pollAndroidEvents();
        if (!gRunning)
            break;

        if (gEglReady && !appStarted)
        {
            platformSetAndroidContext(app->activity->assetManager, app->activity->internalDataPath);
            platformInitAssets();

            const std::string modelArg = resolveLaunchModelArg(0, nullptr);
            if (modelArg.empty())
            {
                LOGE("No model configured. Put config.json in assets.");
                gRunning = false;
                break;
            }

            AppCallbacks callbacks;
            callbacks.width = gWidth;
            callbacks.height = gHeight;
            callbacks.shouldClose = []() { return !gRunning; };
            callbacks.getTime = []() { return nowSeconds(); };
            callbacks.swapBuffers = []() {
                if (gDisplay != EGL_NO_DISPLAY)
                    eglSwapBuffers(gDisplay, gSurface);
            };
            callbacks.pollEvents = pollAndroidEvents;

            appStarted = true;

            if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress))
            {
                LOGE("Failed to initialize OpenGL ES");
                gRunning = false;
                break;
            }

            runModelLoadingApp(modelArg, callbacks);
            break;
        }

        if (!gEglReady)
        {
            int events = 0;
            android_poll_source* source = nullptr;
            ALooper_pollAll(-1, nullptr, &events, (void**)&source);
            if (source != nullptr)
                source->process(app, source);
        }
    }

    terminateEgl();
}
