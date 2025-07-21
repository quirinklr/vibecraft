# Mindestversion für CMake festlegen
cmake_minimum_required(VERSION 3.10)

# Projektname, Version und Sprache definieren
project(MinecraftProject VERSION 1.0 LANGUAGES CXX)

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
add_executable(Minecraft
    src/main.cpp
    src/Engine.cpp
    src/Window.cpp
    src/VulkanRenderer.cpp
    src/Camera.cpp
    src/GameObject.cpp # <-- DIESE ZEILE HINZUFÜGEN
)

# SCHRITT 3: Include-Verzeichnisse hinzufügen
target_include_directories(Minecraft PUBLIC # <-- PRIVATE zu PUBLIC ändern
    ${Vulkan_INCLUDE_DIRS}
    "${GLFW_ROOT_DIR}/include"
    "${CMAKE_SOURCE_DIR}/libs"
)

# SCHRITT 4: Bibliotheken linken
target_link_libraries(Minecraft PRIVATE
    ${Vulkan_LIBRARIES}
    "${GLFW_ROOT_DIR}/lib-vc2022/glfw3.lib"
)

# =================================================================
# KOPIERE SHADER AUTOMATISCH NACH DEM BUILD
# =================================================================
add_custom_command(TARGET Minecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:Minecraft>/shaders"
    COMMENT "Kopiere Shader in den Build-Ordner"
)