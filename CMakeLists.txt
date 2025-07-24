# Mindestversion für CMake festlegen
cmake_minimum_required(VERSION 3.10)

# Projektname, Version und Sprache definieren
project(VibecraftProject VERSION 1.0 LANGUAGES CXX)

# C++ Standard auf 17 (oder höher) setzen
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Das Vulkan-Paket finden (das funktioniert ja)
find_package(Vulkan REQUIRED)

# ---
# MANUELLE UND DIREKTE GLFW-KONFIGURATION
# Wir umgehen find_package komplett.
# ---
set(GLFW_ROOT_DIR "${CMAKE_SOURCE_DIR}/libs/glfw-3.4.bin.WIN64")

# --- NEUER EXECUTABLE BLOCK ---
# Erstelle die ausführbare Datei aus ALLEN unseren Quellcodedateien
add_executable(Vibecraft
    src/main.cpp
    src/Engine.cpp
    src/Window.cpp
    src/VulkanRenderer.cpp
    src/Camera.cpp
    src/Chunk.cpp 
    src/renderer/alloc/AllocImpl.cpp
    src/renderer/alloc/StbImpl.cpp
    src/renderer/command/CommandManager.cpp
    src/renderer/command/SyncPrimitives.cpp
    src/renderer/core/DeviceContext.cpp
    src/renderer/core/InstanceContext.cpp
    src/renderer/pipeline/DescriptorLayout.cpp
    src/renderer/pipeline/PipelineCache.cpp
    src/renderer/resources/TextureManager.cpp
    src/renderer/resources/UploadHelpers.cpp
    src/renderer/swapchain/SwapChainContext.cpp
    src/renderer/Vertex.cpp 
)

# SCHRITT 3: Include-Verzeichnisse hinzufügen
target_include_directories(Vibecraft PUBLIC # <-- PRIVATE zu PUBLIC ändern
    ${Vulkan_INCLUDE_DIRS}
    "${GLFW_ROOT_DIR}/include"
    "${CMAKE_SOURCE_DIR}/libs"
    ${CMAKE_SOURCE_DIR}/src           # allgemeine includes
    ${CMAKE_SOURCE_DIR}/src/renderer  # neuer Ordner mit UniformBufferObject.h
)

# SCHRITT 4: Bibliotheken linken
target_link_libraries(Vibecraft PRIVATE
    ${Vulkan_LIBRARIES}
    "${GLFW_ROOT_DIR}/lib-vc2022/glfw3.lib"
)

# =================================================================
# KOPIERE SHADER AUTOMATISCH NACH DEM BUILD
# =================================================================
add_custom_command(TARGET Vibecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:Vibecraft>/shaders"
    COMMENT "Kopiere Shader in den Build-Ordner"
)

add_custom_command(TARGET Vibecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/textures"
    "$<TARGET_FILE_DIR:Vibecraft>/textures"
    COMMENT "Kopiere Texturen in den Build-Ordner"
)