cmake_minimum_required(VERSION 3.10)
project(VibecraftProject VERSION 1.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

# --- Shader-Kompilierung einrichten ---
find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc (Vulkan Shader Compiler) wurde nicht gefunden. Stellen Sie sicher, dass das Vulkan SDK installiert und zur PATH-Umgebungsvariable hinzugefügt wurde.")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Finde alle Shader-Quelldateien
file(GLOB_RECURSE SHADER_SOURCES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.rgen"
    "${CMAKE_SOURCE_DIR}/shaders/*.rmiss"
    "${CMAKE_SOURCE_DIR}/shaders/*.rchit"
    "${CMAKE_SOURCE_DIR}/shaders/*.rahit" 
)

set(SHADER_OUTPUT_FILES "")

foreach(SHADER_SOURCE_FILE ${SHADER_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/shaders" ${SHADER_SOURCE_FILE})
    set(SHADER_OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${REL_PATH}.spv")
    
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT_FILE})

    set(COMPILE_COMMAND ${GLSLC_EXECUTABLE} -o ${SHADER_OUTPUT_FILE} ${SHADER_SOURCE_FILE})

    # Korrigierte Bedingung: Erfasst alle raytracing-spezifischen Endungen
    if(SHADER_SOURCE_FILE MATCHES "\\.(rgen|rmiss|rchit|rahit)$")
        list(APPEND COMPILE_COMMAND --target-env=vulkan1.2)
    endif()
    
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}/${REL_PATH}/.."
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling Shader: ${REL_PATH}"
        COMMAND ${COMPILE_COMMAND}
        DEPENDS ${SHADER_SOURCE_FILE}
        VERBATIM
    )
endforeach()

add_custom_target(Shaders DEPENDS ${SHADER_OUTPUT_FILES})
# --- Ende Shader-Kompilierung ---


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

add_dependencies(Vibecraft Shaders)

target_include_directories(Vibecraft PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    "${GLFW_ROOT_DIR}/include"
    "${CMAKE_SOURCE_DIR}/libs"
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
    "${SHADER_OUTPUT_DIR}"
    "$<TARGET_FILE_DIR:Vibecraft>/shaders")

add_custom_command(TARGET Vibecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/textures"
    "$<TARGET_FILE_DIR:Vibecraft>/textures")