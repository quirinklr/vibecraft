# Minecraft Vibe Engine - Ein C++ Voxel-Projekt mit Vulkan

Dieses Projekt ist eine von Grund auf in C++ entwickelte Voxel-Engine, inspiriert von Minecraft, mit einem starken Fokus auf hochleistungsfähige, moderne Grafikprogrammierung mithilfe der Vulkan-API.

## 🚀 Aktuelle Features

*   **Moderne Vulkan-Engine:** Nutzt Vulkan für plattformübergreifende, High-Performance-Grafik. Die Architektur ist sauber in logische Komponenten wie Device-, SwapChain- und Command-Management getrennt.
*   **Dynamisches Chunk-System:** Die Welt ist in Chunks unterteilt, die dynamisch geladen und entladen werden, basierend auf der Entfernung des Spielers.
*   **Multithreaded World Generation:** Die Generierung des Terrains und die Erstellung der Chunk-Meshes werden in separaten Threads ausgeführt, um die Haupt-Anwendung flüssig zu halten.
*   **Prozedurale Terrain-Generierung:** Eine grundlegende Landschaft wird mithilfe von `FastNoiseLite` (Simplex-Noise) erstellt.
*   **Effizientes Rendering:** Jeder Chunk wird als einzelner Vertex/Index-Buffer gerendert, um die Anzahl der Draw-Calls zu minimieren.
*   **First-Person-Kamera:** Eine frei bewegliche Kamera mit Maus-Steuerung ist implementiert.

## 🖼️ Screenshots

*(Hier wäre ein Platz für einen Screenshot des aktuellen Builds. Das Ziel ist es, die visuelle Qualität schrittweise an das folgende Inspirationsbild anzunähern.)*

**Inspirations-Ziel:**
![Das Zielbild einer hochqualitativen Voxel-Landschaft](https.://i.imgur.com/your-image-hash.jpg)

## 🛠️ Verwendete Technologien

*   **Sprache:** C++17
*   **Build-System:** CMake
*   **Grafik-API:** [Vulkan](https://www.vulkan.org/)
*   **Speicherverwaltung:** [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
*   **Fensterverwaltung:** [GLFW](https://www.glfw.org/)
*   **Mathematik:** [GLM](https://github.com/g-truc/glm)
*   **Noise-Generierung:** [FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
*   **Bild-Laden:** [stb_image](https://github.com/nothings/stb)

## ⚙️ Bauen und Ausführen

### Voraussetzungen

*   Ein C++ Compiler, der C++17 unterstützt (z.B. MSVC, GCC, Clang)
*   [CMake](https://cmake.org/download/) (Version 3.10+)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

### Schritte

1.  **Klone das Repository:**
    ```bash
    git clone <repository-url>
    cd <projekt-ordner>
    ```

2.  **Konfiguriere das Projekt mit CMake:**
    ```bash
    cmake -B build
    ```

3.  **Baue das Projekt:**
    ```bash
    cmake --build build
    ```

4.  **Führe die Anwendung aus:**
    Die ausführbare Datei, Shader und Texturen findest du im `build/Debug` (oder einem ähnlichen) Verzeichnis.
    ```bash
    ./build/Debug/MinecraftProject
    ```

## 📄 Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe die `LICENSE`-Datei für weitere Details. *(Du müsstest noch eine LICENSE-Datei hinzufügen)*