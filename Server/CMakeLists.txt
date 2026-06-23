cmake_minimum_required(VERSION 3.31)

# Name of executable
project(Server)

# C
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# {{{ ===== BUILD MODES =====

#### Debug ####

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -Og -fno-omit-frame-pointer)
    add_compile_options(
        -fsanitize=address,undefined
        -fsanitize-recover=address
        -fno-omit-frame-pointer
        -g
        -O0
    )
    add_link_options(
        -fsanitize=address,undefined
    )
endif()

#### Dev ####

if (CMAKE_BUILD_TYPE STREQUAL "Dev")
    add_compile_options(-g -O2 -fno-omit-frame-pointer -DNDEBUG)
endif()

#### Release ####

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-g -O3 -ffast-math -DNDEBUG)
endif()

# }}}

# {{{ ===== DEPENDENCIES =====

include(FetchContent)

if (EMSCRIPTEN)
    set(GLFW_BUILD_WSI_EMSCRIPTEN ON CACHE BOOL "" FORCE)
    set(GLFW_USE_POSIX_TIME OFF CACHE BOOL "" FORCE)
    set(GLFW_USE_EMSCRIPTEN ON CACHE BOOL "" FORCE)

    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_COCOA OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WIN32 OFF CACHE BOOL "" FORCE)
    set(GLFW_USE_OSMESA OFF CACHE BOOL "" FORCE)
endif()

# GLFW
if (NOT EMSCRIPTEN)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# STB
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Freetype
FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-14-1
)
FetchContent_MakeAvailable(freetype)

# Miniaudio
set(MINIAUDIO_PATH "${CMAKE_BINARY_DIR}/_deps/miniaudio/miniaudio.h")
file(DOWNLOAD
        https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
        ${MINIAUDIO_PATH}
        SHOW_PROGRESS
)
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE "${CMAKE_BINARY_DIR}/_deps/miniaudio")

# ENet
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/zpl-c/enet.git
    GIT_TAG master
)
FetchContent_MakeAvailable(enet)

# GLAD
add_subdirectory(external/glad)

# Suppress in Debug
set_source_files_properties(glad/src/glad.c PROPERTIES
    COMPILE_FLAGS "-fno-sanitize=undefined"
)
if(TARGET glfw)
    target_compile_options(glfw PRIVATE -fno-sanitize=undefined)
endif()

# }}}

#### Linking ####

add_executable(${CMAKE_PROJECT_NAME}
    src/main.c

    src/network.c
)

# {{{ ===== LIBRARIES =====

# CPSTD
find_package(cpstd REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE cpstd::cpstd)

# OpenMP, pthread, GLFW, GLAD, STB, FT & ENet
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${enet_SOURCE_DIR}/include)

if (NOT EMSCRIPTEN)
    find_package(OpenMP REQUIRED)
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Threads::Threads glad freetype miniaudio)

# }}}

# {{{ ===== WEB EMSCRIPTEN =====

if (NOT EMSCRIPTEN)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw OpenMP::OpenMP_C)
endif()

if (NOT EMSCRIPTEN)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -mavx2 -mfma)
endif()

if (EMSCRIPTEN)
    set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
    set(SHADERS_DIR "${CMAKE_SOURCE_DIR}/shaders")
    set(SHADERS_CUSTOM_DIR "${CMAKE_SOURCE_DIR}/shaders_custom")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        "SHELL:--preload-file ${ASSETS_DIR}@/assets"
        "SHELL:--preload-file ${SHADERS_DIR}@/shaders"
        # "SHELL:--preload-file ${SHADERS_CUSTOM_DIR}@/shaders_custom"
        "-sSTACK_SIZE=8mb"
	    "-sMIN_WEBGL_VERSION=2"
	    "-sMAX_WEBGL_VERSION=2"
        "-sUSE_GLFW=3"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
    )

    # Debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(Example PRIVATE
            "-sSAFE_HEAP=1"
            "-gsource-map"
            "-sASSERTIONS=2"
            "-sSTACK_SIZE=1mb"
            "-sDISABLE_EXCEPTION_CATCHING=0"
            "-fexceptions"
            "-sGL_DEBUG=1"
            "-g"
        )
    endif()

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".html")
endif()

# }}}
