cmake_minimum_required(VERSION 3.10)
project(model_loading LANGUAGES C CXX)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
endif()

if(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    set(SS_GL_USE_ES_DEFAULT ON)
else()
    set(SS_GL_USE_ES_DEFAULT OFF)
endif()
option(SS_GL_USE_ES "Use OpenGL ES via EGL" ${SS_GL_USE_ES_DEFAULT})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(WIN32)
    link_directories(${CMAKE_SOURCE_DIR}/lib)
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/configuration/root_directory.h.in
    ${CMAKE_BINARY_DIR}/configuration/root_directory.h
)

add_library(STB_IMAGE src/stb_image.cpp)
target_include_directories(STB_IMAGE PRIVATE ${CMAKE_SOURCE_DIR}/includes)

if(SS_GL_USE_ES)
    message(STATUS "Building with OpenGL ES (SS_GL_USE_ES=ON)")
    add_library(GLAD_ES src/glad_es.c)
    target_include_directories(GLAD_ES BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/includes/glad_es)
else()
    add_library(GLAD src/glad.c)
    target_include_directories(GLAD PUBLIC ${CMAKE_SOURCE_DIR}/includes)
endif()

add_executable(model_loading
    src/model_loading.cpp
    src/app_engine.cpp
    src/platform_desktop.cpp
    src/scene_config.cpp
    src/ktx2_image_provider.cpp
)

set_target_properties(model_loading PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(model_loading PRIVATE STB_IMAGE)

# Include order matters:
# - glad_es must beat includes/glad
# - on Linux, system Assimp headers must beat bundled includes/assimp
#   (Assimp 5 uses uint32_t aiString::length; bundled LearnOpenGL headers use
#   size_t — on 64-bit that truncates texture paths by 4 chars, e.g. diffuse.jpg -> use.jpg)
if(SS_GL_USE_ES)
    target_compile_definitions(model_loading PRIVATE SS_GL_USE_ES=1 GLFW_INCLUDE_NONE)
    target_include_directories(model_loading BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/includes/glad_es
    )
    target_link_libraries(model_loading PRIVATE GLAD_ES)
else()
    target_compile_definitions(model_loading PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(model_loading PRIVATE GLAD)
endif()

target_include_directories(model_loading PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_BINARY_DIR}/configuration
)

if(WIN32)
    find_package(GLFW3 REQUIRED)
    find_package(ASSIMP REQUIRED)
    target_link_libraries(model_loading PRIVATE glfw3 opengl32 assimp)
    target_compile_definitions(model_loading PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(model_loading PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)

    find_package(PkgConfig REQUIRED)

    if(SS_GL_USE_ES)
        pkg_check_modules(EGL REQUIRED egl)
        pkg_check_modules(GLES2 REQUIRED glesv2)
        target_include_directories(GLAD_ES PRIVATE ${GLES2_INCLUDE_DIRS})
        target_link_libraries(model_loading PRIVATE ${EGL_LIBRARIES} ${GLES2_LIBRARIES})
        target_link_libraries(GLAD_ES PRIVATE ${GLES2_LIBRARIES})
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(model_loading PRIVATE ${OPENGL_LIBRARIES})
    endif()

    find_package(glfw3 3.3 QUIET)
    if(TARGET glfw)
        target_link_libraries(model_loading PRIVATE glfw)
    else()
        pkg_check_modules(GLFW3 REQUIRED glfw3)
        target_link_libraries(model_loading PRIVATE ${GLFW3_LIBRARIES})
    endif()

    find_package(ASSIMP REQUIRED)
    if(NOT ASSIMP_INCLUDE_DIR)
        message(FATAL_ERROR "ASSIMP_INCLUDE_DIR not set")
    endif()
    # Prefer system Assimp headers over bundled includes/assimp (ABI must match libassimp).
    get_filename_component(_assimp_inc_abs "${ASSIMP_INCLUDE_DIR}" ABSOLUTE)
    get_filename_component(_bundled_inc_abs "${CMAKE_SOURCE_DIR}/includes" ABSOLUTE)
    if(_assimp_inc_abs STREQUAL _bundled_inc_abs)
        message(FATAL_ERROR
            "Assimp headers resolved to bundled LearnOpenGL includes/assimp, which do not match "
            "system libassimp (aiString::length size_t vs uint32_t). Install matching headers:\n"
            "  sudo apt-get install -y libassimp-dev\n"
            "Then wipe build/ and reconfigure.")
    endif()
    target_include_directories(model_loading BEFORE PRIVATE ${ASSIMP_INCLUDE_DIR})
    message(STATUS "Assimp library: ${ASSIMP_LIBRARY}")
    message(STATUS "Assimp include: ${ASSIMP_INCLUDE_DIR}")
    target_link_libraries(model_loading PRIVATE ${ASSIMP_LIBRARIES})
elseif(APPLE)
    find_package(GLFW3 REQUIRED)
    find_package(ASSIMP REQUIRED)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(OpenGL_LIBRARY OpenGL)
    find_library(IOKit_LIBRARY IOKit)
    find_library(CoreVideo_LIBRARY CoreVideo)
    target_link_libraries(model_loading PRIVATE
        GLAD
        ${GLFW3_LIBRARY}
        ${ASSIMP_LIBRARIES}
        ${COCOA_LIBRARY}
        ${IOKit_LIBRARY}
        ${OpenGL_LIBRARY}
        ${CoreVideo_LIBRARY}
    )
endif()

# FBX -> OBJ helper for asset conversion (fy scene)
if(ASSIMP_FOUND OR WIN32 OR (UNIX AND NOT APPLE) OR APPLE)
    if(NOT ASSIMP_FOUND)
        find_package(ASSIMP REQUIRED)
    endif()
    add_executable(fbx2obj tools/fbx2obj.cpp)
    set_target_properties(fbx2obj PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )
    if(ASSIMP_INCLUDE_DIR)
        target_include_directories(fbx2obj BEFORE PRIVATE ${ASSIMP_INCLUDE_DIR})
    else()
        target_include_directories(fbx2obj BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/includes)
    endif()
    if(WIN32)
        target_link_libraries(fbx2obj PRIVATE assimp)
    else()
        target_link_libraries(fbx2obj PRIVATE ${ASSIMP_LIBRARIES})
    endif()
    if(MSVC)
        target_compile_options(fbx2obj PRIVATE /std:c++17)
    endif()

    find_program(PYTHON3_EXECUTABLE NAMES python3 python)
    if(PYTHON3_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/new-resources/fy/mesh")
        set(FY_CONVERT_STAMP "${CMAKE_BINARY_DIR}/fy_convert.stamp")
        add_custom_command(
            OUTPUT "${FY_CONVERT_STAMP}"
            COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/convert_fy_for_load_model.py"
            COMMAND ${CMAKE_COMMAND} -E touch "${FY_CONVERT_STAMP}"
            DEPENDS fbx2obj
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Converting fy RFS assets to load-model layout"
        )
        add_custom_target(convert_fy_assets DEPENDS "${FY_CONVERT_STAMP}")
        add_dependencies(model_loading convert_fy_assets)
    endif()
endif()

if(MSVC)
    target_compile_options(model_loading PRIVATE /std:c++17 /MP)
    target_link_options(model_loading PUBLIC /ignore:4099)
endif()

if(WIN32)
    file(GLOB RUNTIME_DLLS "${CMAKE_SOURCE_DIR}/dlls/*.dll")
    foreach(SHADER ${CMAKE_SOURCE_DIR}/shaders/1.model_loading.vs
                   ${CMAKE_SOURCE_DIR}/shaders/1.model_loading.fs)
        add_custom_command(TARGET model_loading PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER} $<TARGET_FILE_DIR:model_loading>)
    endforeach()
    foreach(DLL ${RUNTIME_DLLS})
        add_custom_command(TARGET model_loading PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL} $<TARGET_FILE_DIR:model_loading>)
    endforeach()
    add_custom_command(TARGET model_loading PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources/objects/backpack
            $<TARGET_FILE_DIR:model_loading>/backpack)
    add_custom_command(TARGET model_loading PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.json
            $<TARGET_FILE_DIR:model_loading>/config.json)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/objects/fy/fy.obj"
       OR EXISTS "${CMAKE_SOURCE_DIR}/resources/objects/fy/scene.json")
        add_custom_command(TARGET model_loading PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources/objects/fy
                $<TARGET_FILE_DIR:model_loading>/fy)
    endif()
else()
    if(SS_GL_USE_ES)
        set(SHADER_VS ${CMAKE_SOURCE_DIR}/shaders/1.model_loading_es.vs)
        set(SHADER_FS ${CMAKE_SOURCE_DIR}/shaders/1.model_loading_es.fs)
    else()
        set(SHADER_VS ${CMAKE_SOURCE_DIR}/shaders/1.model_loading.vs)
        set(SHADER_FS ${CMAKE_SOURCE_DIR}/shaders/1.model_loading.fs)
    endif()
    add_custom_command(TARGET model_loading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_VS} $<TARGET_FILE_DIR:model_loading>/1.model_loading.vs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_FS} $<TARGET_FILE_DIR:model_loading>/1.model_loading.fs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.json $<TARGET_FILE_DIR:model_loading>/config.json
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources/objects/backpack
            $<TARGET_FILE_DIR:model_loading>/backpack)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/objects/fy/fy.obj"
       OR EXISTS "${CMAKE_SOURCE_DIR}/resources/objects/fy/scene.json")
        add_custom_command(TARGET model_loading POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/resources/objects/fy
                $<TARGET_FILE_DIR:model_loading>/fy)
    endif()
    if(SS_GL_USE_ES)
        add_custom_command(TARGET model_loading POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER_VS} $<TARGET_FILE_DIR:model_loading>/1.model_loading_es.vs
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER_FS} $<TARGET_FILE_DIR:model_loading>/1.model_loading_es.fs)
    endif()
endif()
