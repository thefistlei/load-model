cmake_minimum_required(VERSION 3.22.1)
project(model_loading_android)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep Assimp static so the APK only needs libmodel_loading.so
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
FetchContent_MakeAvailable(assimp)

add_library(app-glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(app-glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)
# Glue provides ANativeActivity_onCreate; keep it even if "unused"
target_compile_options(app-glue PRIVATE -fPIC)

add_library(model_loading SHARED
    android_main.cpp
    ${PROJECT_ROOT}/src/app_engine.cpp
    ${PROJECT_ROOT}/src/platform_android.cpp
    ${PROJECT_ROOT}/src/scene_config.cpp
    ${PROJECT_ROOT}/src/ktx2_image_provider.cpp
    ${PROJECT_ROOT}/src/stb_image.cpp
    ${PROJECT_ROOT}/src/glad_es.c
)

set_target_properties(model_loading PROPERTIES
    OUTPUT_NAME "model_loading"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

target_compile_definitions(model_loading PRIVATE
    SS_GL_USE_ES=1
)

target_include_directories(model_loading BEFORE PRIVATE
    ${PROJECT_ROOT}/includes/glad_es
)

target_include_directories(model_loading PRIVATE
    ${PROJECT_ROOT}/includes
)

find_library(log-lib log)
find_library(android-lib android)
find_library(EGL-lib EGL)
find_library(GLESv3-lib GLESv3)

target_link_libraries(model_loading PRIVATE
    assimp
    app-glue
    ${log-lib}
    ${android-lib}
    ${EGL-lib}
    ${GLESv3-lib}
)

# NativeActivity looks up ANativeActivity_onCreate in this .so
target_link_options(model_loading PRIVATE
    -uANativeActivity_onCreate
)
