cmake_minimum_required(VERSION 3.20)
project(ProjetoICG LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Avisos
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# GLAD (gera os arquivos em third_party/glad antes; aqui só declara a lib)
if(NOT TARGET glad)
    add_library(glad STATIC third_party/glad/src/glad.c)
    target_include_directories(glad PUBLIC third_party/glad/include)
    if(WIN32)
        target_compile_definitions(glad PUBLIC GLAD_GL)
    endif()
endif()

# Fontes do projeto
set(COMMON_SOURCES
    src/common/blocks.cpp
    src/common/config.cpp
)

set(BACKEND_SOURCES
    src/backend/player.cpp
    src/backend/world.cpp
    src/backend/generator.cpp
    src/backend/lighting.cpp
    src/backend/mesher.cpp
    src/backend/raycast.cpp
    src/backend/save.cpp
    src/backend/backend.cpp
)

set(CLIENT_SOURCES
    src/client/main.cpp
    src/client/app.cpp
    src/client/window.cpp
    src/client/camera.cpp
    src/client/renderer.cpp
    src/client/shaders.cpp
    src/client/textures.cpp
    src/client/hud.cpp
    src/client/skybox.cpp
)

# Filtrar apenas arquivos existentes (evita erro de configure)
set(ALL_SOURCES)
foreach(f ${COMMON_SOURCES} ${BACKEND_SOURCES} ${CLIENT_SOURCES})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        list(APPEND ALL_SOURCES ${f})
    endif()
endforeach()

add_executable(projeto_icg ${ALL_SOURCES})

target_include_directories(projeto_icg PRIVATE
    src
    src/common
    src/backend
    src/client
    third_party
    third_party/stb
    third_party/FastNoiseLite
)

target_link_libraries(projeto_icg PRIVATE
    glfw
    glad
    glm::glm
    nlohmann_json::nlohmann_json
)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(projeto_icg PRIVATE OpenGL::GL)

# Threads
find_package(Threads REQUIRED)
target_link_libraries(projeto_icg PRIVATE Threads::Threads)

# Copiar config e biomas para o diretório do executável
add_custom_command(TARGET projeto_icg POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:projeto_icg>/config.json
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/biomas
        $<TARGET_FILE_DIR:projeto_icg>/biomas
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:projeto_icg>/assets
)