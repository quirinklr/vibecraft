cmake_minimum_required(VERSION 3.10)
project(VibecraftProject VERSION 1.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

set(GLFW_ROOT_DIR "${CMAKE_SOURCE_DIR}/libs/glfw-3.4.bin.WIN64")

find_library(
    GLFW_LIBRARY_FILE
    NAMES glfw3 glfw
    HINTS "${GLFW_ROOT_DIR}"
    PATH_SUFFIXES lib lib-vc2022 lib-vc2019 lib-mingw-w64
)

if(NOT GLFW_LIBRARY_FILE)
    message(FATAL_ERROR "CMake konnte die GLFW-Bibliotheksdatei (glfw3.lib) nicht im Verzeichnis ${GLFW_ROOT_DIR} finden. Bitte stellen Sie sicher, dass die Bibliothek vorhanden ist.")
endif()

file(GLOB_RECURSE VIBECRAFT_SRC CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(Vibecraft ${VIBECRAFT_SRC})

target_include_directories(Vibecraft PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    "${GLFW_ROOT_DIR}/include"
    "${CMAKE_SOURCE_DIR}/libs/noise"
    "${CMAKE_SOURCE_DIR}/libs/vma"
    "${CMAKE_SOURCE_DIR}/libs/stb"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/renderer"
    "${CMAKE_SOURCE_DIR}/src/generation"
)

target_link_libraries(Vibecraft PRIVATE
    ${Vulkan_LIBRARIES}
    ${GLFW_LIBRARY_FILE}
)

add_custom_command(TARGET Vibecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:Vibecraft>/shaders")

add_custom_command(TARGET Vibecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/textures"
    "$<TARGET_FILE_DIR:Vibecraft>/textures")