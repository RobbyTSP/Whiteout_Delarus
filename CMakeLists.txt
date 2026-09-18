cmake_minimum_required(VERSION 3.24)
project(WhiteoutDelarus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and optimizations
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# GLM (header-only)
find_path(GLM_INCLUDE_DIR glm/glm.hpp PATHS /usr/include /usr/local/include)
if (NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM headers not found!")
endif()

# STB Image (header-only)
find_path(STB_INCLUDE_DIR stb/stb_image.h PATHS /usr/include /usr/local/include)

# Find Slang compiler (slangc)
find_program(SLANGC_EXECUTABLE slangc PATHS /usr/local/bin /usr/bin)
if (NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc compiler not found! Please ensure slangc is installed on PATH.")
endif()
message(STATUS "Found Slang compiler: ${SLANGC_EXECUTABLE}")

# Shader compilation helper
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SPV_OUTPUT_DIR})

# Function to compile Slang shaders to SPIR-V
function(add_slang_shader SHADER_NAME STAGE ENTRY_POINT OUTPUT_NAME)
    set(INPUT_FILE "${SHADER_DIR}/${SHADER_NAME}")
    set(OUTPUT_FILE "${SPV_OUTPUT_DIR}/${OUTPUT_NAME}")

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${SLANGC_EXECUTABLE} ${INPUT_FILE}
                -target spirv
                -stage ${STAGE}
                -entry ${ENTRY_POINT}
                -profile glsl_460
                -o ${OUTPUT_FILE}
        DEPENDS ${INPUT_FILE}
        COMMENT "Compiling Slang shader ${SHADER_NAME} [${STAGE} -> ${ENTRY_POINT}] to ${OUTPUT_NAME}"
    )
    list(APPEND SPV_TARGETS ${OUTPUT_FILE})
    set(SPV_TARGETS ${SPV_TARGETS} PARENT_SCOPE)
endfunction()

# Register Slang shaders
add_slang_shader("terrain.slang" "vertex" "vertexMain" "terrain_vert.spv")
add_slang_shader("terrain.slang" "fragment" "fragmentMain" "terrain_frag.spv")
add_slang_shader("sky.slang" "vertex" "vertexMain" "sky_vert.spv")
add_slang_shader("sky.slang" "fragment" "fragmentMain" "sky_frag.spv")
add_slang_shader("histogram.slang" "compute" "computeMain" "histogram_comp.spv")
add_slang_shader("adapt_exposure.slang" "compute" "computeMain" "adapt_exposure_comp.spv")
add_slang_shader("postprocess.slang" "vertex" "vertexMain" "postprocess_vert.spv")
add_slang_shader("postprocess.slang" "fragment" "fragmentMain" "postprocess_frag.spv")
add_slang_shader("micro_terrain.slang" "vertex" "vertexMain" "micro_terrain_vert.spv")
add_slang_shader("micro_terrain.slang" "fragment" "fragmentMain" "micro_terrain_frag.spv")
add_slang_shader("boulder.slang" "vertex" "vertexMain" "boulder_vert.spv")
add_slang_shader("boulder.slang" "fragment" "fragmentMain" "boulder_frag.spv")
add_slang_shader("multi_bounce_gi.slang" "compute" "computeMain" "multi_bounce_gi_comp.spv")
add_slang_shader("snow_physics.slang" "compute" "computeMain" "snow_physics_comp.spv")
add_slang_shader("avalanche_physics.slang" "compute" "computeMain" "avalanche_physics_comp.spv")
add_slang_shader("avalanche.slang" "vertex" "vertexMain" "avalanche_vert.spv")
add_slang_shader("avalanche.slang" "fragment" "fragmentMain" "avalanche_frag.spv")
add_slang_shader("gaussian_splat.slang" "vertex" "vertexMain" "gaussian_splat_vert.spv")
add_slang_shader("gaussian_splat.slang" "fragment" "fragmentMain" "gaussian_splat_frag.spv")
add_slang_shader("ladder.slang" "vertex" "vertexMain" "ladder_vert.spv")
add_slang_shader("ladder.slang" "fragment" "fragmentMain" "ladder_frag.spv")

add_custom_target(compile_shaders ALL DEPENDS ${SPV_TARGETS})

# Application Sources
set(SOURCES
    src/main.cpp
    src/core/Window.cpp
    src/core/Camera.cpp
    src/rhi/VulkanContext.cpp
    src/rhi/VulkanSwapchain.cpp
    src/rhi/VulkanShader.cpp
    src/rhi/VulkanPipeline.cpp
    src/rhi/VulkanComputePipeline.cpp
    src/rhi/VulkanBuffer.cpp
    src/rhi/VulkanTexture.cpp
    src/renderer/Renderer.cpp
    src/game/TerrainCollider.cpp
    src/game/Player.cpp
    src/game/WeatherSystem.cpp
    src/game/SnowpackSimulation.cpp
    src/game/CrevasseBridgeSystem.cpp
    src/audio/AcousticRaytracer.cpp
    src/audio/AlpineAudioEngine.cpp
)

set(HEADERS
    src/core/Window.hpp
    src/core/Camera.hpp
    src/core/Timer.hpp
    src/rhi/VulkanContext.hpp
    src/rhi/VulkanSwapchain.hpp
    src/rhi/VulkanShader.hpp
    src/rhi/VulkanPipeline.hpp
    src/rhi/VulkanComputePipeline.hpp
    src/rhi/VulkanBuffer.hpp
    src/rhi/VulkanTexture.hpp
    src/renderer/Renderer.hpp
    src/game/TerrainCollider.hpp
    src/game/Player.hpp
    src/game/WeatherSystem.hpp
    src/game/SnowpackSimulation.hpp
    src/game/CrevasseBridgeSystem.hpp
    src/audio/AcousticRaytracer.hpp
    src/audio/AlpineAudioEngine.hpp
)

add_executable(whiteout ${SOURCES} ${HEADERS})
add_dependencies(whiteout compile_shaders)

target_include_directories(whiteout PRIVATE
    src
    ${Vulkan_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIR}
)

if (STB_INCLUDE_DIR)
    target_include_directories(whiteout PRIVATE ${STB_INCLUDE_DIR})
endif()

target_compile_definitions(whiteout PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    SHADER_DIR="${SPV_OUTPUT_DIR}"
    DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(whiteout PRIVATE
    Vulkan::Vulkan
    ${SDL2_LIBRARIES}
)
