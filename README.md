# Vibecraft Engine - Ein C++ Voxel-Projekt mit Vulkan

Dieses Projekt ist eine von Grund auf in C++20 entwickelte Voxel-Engine, inspiriert von Minecraft. Der Fokus liegt auf einer hochperformanten, modernen und sauberen Architektur, die die Fähigkeiten der Vulkan-API voll ausnutzt.

## 🚀 Key Features

*   **Hocheffiziente Rendering-Pipeline mit Vulkan:** Nutzt moderne Vulkan-Konzepte für plattformübergreifende, leistungsstarke Grafik. Die Architektur ist sauber in logische Komponenten wie Device-, SwapChain- und Command-Management getrennt.

*   **Vollständig asynchrones Chunk-System:**
    *   **Ruckelfreies Laden:** Die Welt ist in Chunks unterteilt, die dynamisch geladen werden. Ein `std::jthread`-basierter Thread-Pool übernimmt die rechenintensive Terrain-Generierung, das Meshing und die Vorbereitung der GPU-Daten im Hintergrund.
    *   **Asynchrones GPU-Staging:** Die Vertex-Daten werden in Worker-Threads in Staging-Buffer kopiert, wodurch der Haupt-Thread von `memcpy`-Operationen entlastet und eine maximal flüssige Framerate gewährleistet wird.

*   **Optimierte Voxel-Darstellung:**
    *   **Greedy Meshing:** Reduziert die Vertex-Anzahl von Chunks drastisch, indem benachbarte, identische Blockflächen zu großen Polygonen zusammengefasst werden.
    *   **Frustum Culling:** Entlastet die GPU signifikant, indem nur die Chunks gerendert werden, die sich tatsächlich im Sichtfeld der Kamera befinden.

*   **Modulare, erweiterbare Welt-Generierung:**
    *   **Layered Noise:** Nutzt `FastNoiseLite` (OpenSimplex2/Perlin), um mittels mehrerer überlagerter Noise-Maps (für Kontinente, Erosion, Höhlen) abwechslungsreiches Terrain zu erzeugen.
    *   **Biom-System:** Ein sauberes, erweiterbares System basierend auf Temperatur-Maps, das verschiedene Biome wie Ebenen, Wüsten und Ozeane mit jeweils eigenen Oberflächenregeln erzeugt.

*   **Moderne C++-Architektur:**
    *   Entwickelt in **C++20** mit modernen Features.
    *   Sicheres Ressourcen-Management durch RAII-Wrapper für alle Vulkan-Handles.

## 🛠️ Verwendete Technologien

*   **Sprache:** C++20
*   **Build-System:** CMake
*   **Grafik-API:** [Vulkan](https://www.vulkan.org/)
*   **Speicherverwaltung:** [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
*   **Fensterverwaltung:** [GLFW](https://www.glfw.org/)
*   **Mathematik:** [GLM](https://github.com/g-truc/glm)
*   **Noise-Generierung:** [FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
*   **Bild-Laden:** [stb_image](https://github.com/nothings/stb)

## ⚙️ Bauen und Ausführen

### Voraussetzungen

*   Ein C++ Compiler, der C++20 unterstützt (z.B. MSVC, GCC, Clang)
*   [CMake](https://cmake.org/download/) (Version 3.10+)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

### Schritte

1.  **Klone das Repository:**
    ```bash
    git clone https://github.com/quirinklr/minecraft-vibe.git
    cd minecraft-vibe
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
    ./build/Debug
    ```

## 📄 Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe die `LICENSE`-Datei für weitere Details.