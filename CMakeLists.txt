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

# SCHRITT 1: Pfad zum Hauptordner von GLFW definieren.
# BITTE ÜBERPRÜFE DEN ORDNERNAMEN "glfw-3.4.bin.WIN64" GANZ GENAU IN DEINEM EXPLORER!
set(GLFW_ROOT_DIR "${CMAKE_SOURCE_DIR}/libs/glfw-3.4.bin.WIN64")

# SCHRITT 2: Eine ausführbare Datei erstellen
add_executable(Minecraft src/main.cpp)

# SCHRITT 3: Include-Verzeichnisse hinzufügen
# Dies sagt dem Compiler, wo er die Header-Dateien wie <GLFW/glfw3.h> finden kann.
target_include_directories(Minecraft PRIVATE
    # Der Include-Pfad für Vulkan (wird von find_package gesetzt)
    ${Vulkan_INCLUDE_DIRS}
    # Der Include-Pfad für GLFW (den wir manuell angeben)
    "${GLFW_ROOT_DIR}/include"
)

# SCHRITT 4: Bibliotheken linken
# Dies sagt dem Linker, welche .lib-Dateien er braucht.
target_link_libraries(Minecraft PRIVATE
    # Die Vulkan-Bibliothek (wird von find_package gesetzt)
    ${Vulkan_LIBRARIES}
    # Die GLFW-Bibliothek (die wir manuell und direkt angeben)
    # BITTE ÜBERPRÜFE, DASS DER ORDNER "lib-vc2022" HEISST UND DIE DATEI "glfw3.lib" EXISTIERT!
    "${GLFW_ROOT_DIR}/lib-vc2022/glfw3.lib"
)

# =================================================================
# KOPIERE SHADER AUTOMATISCH NACH DEM BUILD
# =================================================================
# Dieser Befehl wird jedes Mal ausgeführt, NACHDEM das Ziel "Minecraft"
# (deine .exe) erfolgreich erstellt wurde.
add_custom_command(TARGET Minecraft POST_BUILD
    # Wir benutzen das eingebaute CMake-Kommando, um ein Verzeichnis zu kopieren.
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    # QUELLE: Der "shaders"-Ordner im Hauptverzeichnis deines Projekts.
    "${CMAKE_SOURCE_DIR}/shaders"
    # ZIEL: Ein neuer "shaders"-Ordner direkt neben der fertigen .exe-Datei.
    "$<TARGET_FILE_DIR:Minecraft>/shaders"
    COMMENT "Kopiere Shader in den Build-Ordner"
)