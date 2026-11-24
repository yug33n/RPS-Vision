cmake_minimum_required(VERSION 3.30)

# Auto-detect vcpkg toolchain if not passed via -DCMAKE_TOOLCHAIN_FILE
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    elseif(DEFINED ENV{VSINSTALLDIR} AND EXISTS "$ENV{VSINSTALLDIR}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VSINSTALLDIR}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(RPS_Vision
    VERSION 0.1.0
    DESCRIPTION "Real-time AI Rock-Paper-Scissors desktop application"
    LANGUAGES C CXX
)

# Enforce C++23 globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RPS_VISION_LINK_DEPS "Find and link external dependencies from vcpkg / FetchContent" ON)

set(RPS_VISION_SOURCES
    src/main.cpp
    src/core/AppConfig.cpp
    src/vision/CameraCapture.cpp
    src/vision/HandTracker.cpp
    src/vision/GestureClassifier.cpp
    src/game/MarkovPredictor.cpp
    src/game/GameEngine.cpp
    src/ai/TemplateCommentator.cpp
    src/ai/LlamaCommentator.cpp
    src/audio/AudioEngine.cpp
    src/ui/TextureManager.cpp
    src/ui/Theme.cpp
    src/ui/ArenaUI.cpp
)

add_executable(RPS_Vision ${RPS_VISION_SOURCES})
set_target_properties(RPS_Vision PROPERTIES
    OUTPUT_NAME "RPS_Vision"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(RPS_Vision PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_compile_definitions(RPS_Vision PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(MSVC)
    target_compile_options(RPS_Vision PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(RPS_Vision PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RPS_VISION_LINK_DEPS)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(implot CONFIG REQUIRED)
    find_package(OpenCV CONFIG REQUIRED COMPONENTS core imgproc videoio)
    find_package(nlohmann_json CONFIG REQUIRED)

    # ONNX Runtime: check vcpkg/CMAKE_PREFIX_PATH first, then third_party/onnxruntime or ONNXRUNTIME_ROOT
    find_package(onnxruntime CONFIG QUIET)
    set(RPS_ONNXRUNTIME_DLL "")
    if(onnxruntime_FOUND)
        set(RPS_HAS_ONNXRUNTIME 1)
    else()
        set(_ORT_SEARCH_ROOTS
            "$ENV{ONNXRUNTIME_ROOT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime"
        )
        find_path(ORT_INCLUDE_DIR
            NAMES onnxruntime_cxx_api.h
            PATHS ${_ORT_SEARCH_ROOTS}
            PATH_SUFFIXES include build/native/include
            NO_DEFAULT_PATH
        )
        find_library(ORT_LIBRARY
            NAMES onnxruntime
            PATHS ${_ORT_SEARCH_ROOTS}
            PATH_SUFFIXES lib runtimes/win-x64/native
            NO_DEFAULT_PATH
        )
        find_file(ORT_DLL
            NAMES onnxruntime.dll
            PATHS ${_ORT_SEARCH_ROOTS}
            PATH_SUFFIXES bin lib runtimes/win-x64/native
            NO_DEFAULT_PATH
        )
        if(ORT_INCLUDE_DIR AND ORT_LIBRARY)
            add_library(onnxruntime::onnxruntime UNKNOWN IMPORTED)
            set_target_properties(onnxruntime::onnxruntime PROPERTIES
                IMPORTED_LOCATION "${ORT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
            )
            if(ORT_DLL)
                set(RPS_ONNXRUNTIME_DLL "${ORT_DLL}")
            endif()
            set(RPS_HAS_ONNXRUNTIME 1)
            message(STATUS "Found local ONNX Runtime: ${ORT_LIBRARY}")
        else()
            set(RPS_HAS_ONNXRUNTIME 0)
            message(WARNING "ONNX Runtime not found; building with CV inference stub until third_party/onnxruntime is provided.")
        endif()
    endif()
    target_compile_definitions(RPS_Vision PRIVATE RPS_HAS_ONNXRUNTIME=${RPS_HAS_ONNXRUNTIME})

    # llama.cpp: prefer vcpkg package, fallback to FetchContent (v1.0.0)
    find_package(llama CONFIG QUIET)
    if(NOT llama_FOUND)
        include(FetchContent)
        message(STATUS "llama-cpp not found via find_package; using FetchContent (ggml-org/llama.cpp v1.0.0)")
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            llama_cpp
            GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
            GIT_TAG        v1.0.0
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(llama_cpp)
    endif()

    # miniaudio: prefer vcpkg package/header, fallback to FetchContent (0.11.26)
    find_package(miniaudio CONFIG QUIET)
    if(NOT miniaudio_FOUND)
        find_path(MINIAUDIO_INCLUDE_DIRS NAMES miniaudio.h)
        if(MINIAUDIO_INCLUDE_DIRS)
            add_library(miniaudio_header INTERFACE)
            target_include_directories(miniaudio_header INTERFACE "${MINIAUDIO_INCLUDE_DIRS}")
            add_library(miniaudio::miniaudio ALIAS miniaudio_header)
        else()
            include(FetchContent)
            message(STATUS "miniaudio not found via vcpkg; using FetchContent (mackron/miniaudio 0.11.26)")
            set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            FetchContent_Declare(
                miniaudio
                GIT_REPOSITORY https://github.com/mackron/miniaudio.git
                GIT_TAG        0.11.26
                GIT_SHALLOW    TRUE
            )
            FetchContent_MakeAvailable(miniaudio)
        endif()
    endif()

    target_link_libraries(RPS_Vision PRIVATE
        OpenGL::GL
        glfw
        imgui::imgui
        $<$<TARGET_EXISTS:implot::implot>:implot::implot>
        $<$<NOT:$<TARGET_EXISTS:implot::implot>>:implot>
        opencv_core
        opencv_imgproc
        opencv_videoio
        nlohmann_json::nlohmann_json
        $<$<TARGET_EXISTS:llama::llama>:llama::llama>
        $<$<NOT:$<TARGET_EXISTS:llama::llama>>:llama>
        $<$<TARGET_EXISTS:miniaudio::miniaudio>:miniaudio::miniaudio>
        $<$<NOT:$<TARGET_EXISTS:miniaudio::miniaudio>>:miniaudio>
    )

    if(RPS_HAS_ONNXRUNTIME)
        target_link_libraries(RPS_Vision PRIVATE onnxruntime::onnxruntime)
        if(RPS_ONNXRUNTIME_DLL AND EXISTS "${RPS_ONNXRUNTIME_DLL}")
            add_custom_command(TARGET RPS_Vision POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${RPS_ONNXRUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:RPS_Vision>/onnxruntime.dll"
                COMMENT "Copying onnxruntime.dll to output folder"
            )
        endif()
    endif()

    if(WIN32)
        # Windows SAPI 5.4 COM linkage for local voice synthesis
        target_link_libraries(RPS_Vision PRIVATE ole32)
    endif()
endif()

# Ensure runtime directories and assets are prepared next to the executable
add_custom_command(TARGET RPS_Vision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RPS_Vision>/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RPS_Vision>/models"
    COMMENT "Preparing runtime directories for RPS_Vision"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET RPS_Vision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:RPS_Vision>/assets"
        COMMENT "Copying assets directory to output folder"
    )
endif()
