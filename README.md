# Vibecraft Engine - Ein C++ Voxel-Projekt mit Vulkan

Dieses Projekt ist eine von Grund auf in C++20 entwickelte Voxel-Engine, inspiriert von Minecraft. Der Fokus liegt auf einer hochperformanten, modernen und sauberen Architektur, die die Fähigkeiten der Vulkan-API voll ausnutzt und eine interaktive, prozedural generierte Welt simuliert.

## 🚀 Key Features

*   **Hocheffiziente Rendering-Pipeline mit Vulkan:** Nutzt moderne Vulkan-Konzepte für plattformübergreifende, leistungsstarke Grafik. Die Architektur ist sauber in logische Komponenten wie Device-, SwapChain- und Command-Management getrennt.

*   **Vollständig asynchrones & priorisiertes Chunk-System:**
    *   **Intelligentes, ruckelfreies Laden:** Die Welt wird basierend auf der Entfernung zum Spieler geladen und gemesht – von innen nach außen. Dies sorgt für ein nahtloses Spielerlebnis ohne sichtbare Lücken bei schnellen Bewegungen.
    *   **Effizienter Thread-Pool:** Ein `std::jthread`-basierter Thread-Pool übernimmt rechenintensive Terrain-Generierung, Meshing und GPU-Datenvorbereitung im Hintergrund, ohne die Framerate zu beeinträchtigen.

*   **Interaktive Spielwelt & Spieler-Controller:**
    *   **Physik-basierter Charakter:** Anstelle einer simplen Fliege-Kamera steuerst du einen echten Charakter mit Schwerkraft, Sprung-Mechanik und einer AABB-basierten Kollisionserkennung mit der Welt.
    *   **Block-Interaktion:** Zerstöre Blöcke in Echtzeit mittels Ray-Casting. Dank eines "Swap-on-ready"-Mechanismus werden Chunks **flicker-frei** aktualisiert, sobald das neue Mesh auf der GPU bereit ist.
    *   **Erweiterbares Entity-System:** Eine saubere `Entity`-Basisklasse ermöglicht die einfache Hinzufügung neuer Kreaturen (Zombies, Tiere etc.) mit eigener Logik in der Zukunft.

*   **Optimierte Voxel-Darstellung:**
    *   **Greedy Meshing:** Reduziert die Vertex-Anzahl von Chunks drastisch, indem benachbarte, identische Blockflächen zu großen Polygonen zusammengefasst werden.
    *   **Frustum Culling:** Entlastet die GPU signifikant, indem nur die Chunks gerendert werden, die sich tatsächlich im Sichtfeld der Kamera befinden.

*   **Modulare Welt-Generierung:**
    *   **Layered Noise:** Nutzt `FastNoiseLite` (OpenSimplex2/Perlin), um mittels mehrerer überlagerter Noise-Maps (für Kontinente, Erosion, Höhlen) abwechslungsreiches Terrain zu erzeugen.
    *   **Biom-System:** Ein sauberes, erweiterbares System basierend auf Temperatur-Maps erzeugt verschiedene Biome wie Ebenen, Wüsten und Ozeane.

## 🛠️ Verwendete Technologien

*   **Sprache:** C++20
*   **Build-System:** CMake
*   **Grafik-API:** [Vulkan](https://www.vulkan.org/)
*   **Speicherverwaltung:** [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
*   **Fensterverwaltung:** [GLFW](https://www.glfw.org/)
*   **Mathematik:** [GLM](https://github.com/g-truc/glm)
*   **Noise-Generierung:** [FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
*   **Bild-Laden:** [stb_image](https://github.com/nothings/stb)

## ⚙️ Ausführen & Bauen

### 🎮 Für Spieler (Release-Version)

1.  Gehe zur [**Releases-Seite**](https://github.com/quirinklr/minecraft-vibe/releases) des Projekts.
2.  Lade die `.zip`-Datei des neuesten Releases herunter (z.B. `Vibecraft-v1.0.0-win64.zip`).
3.  **WICHTIG:** Installiere die [**Microsoft Visual C++ Redistributable (x64)**](https://aka.ms/vs/17/release/vc_redist.x64.exe). Dies ist nur einmal erforderlich.
4.  Entpacke die ZIP-Datei an einem beliebigen Ort.
5.  Starte `Vibecraft.exe`.

### 👨‍💻 Für Entwickler (Aus dem Quellcode)

#### Voraussetzungen

*   Ein C++ Compiler, der C++20 unterstützt (z.B. MSVC, GCC, Clang)
*   [CMake](https://cmake.org/download/) (Version 3.10+)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

#### Schritte

1.  **Klone das Repository:**
    ```bash
    git clone https://github.com/quirinklr/minecraft-vibe.git
    cd minecraft-vibe
    ```

2.  **Konfiguriere das Projekt mit CMake:**
    ```bash
    cmake -B build
    ```

3.  **Baue das Projekt (empfohlen im Release-Modus):**
    ```bash
    cmake --build build --config Release
    ```

4.  **Führe die Anwendung aus:**
    Die ausführbare Datei findest du im `build/Release`-Verzeichnis.
    ```bash
    ./build/Release/Vibecraft.exe
    ```

## 📄 Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe die `LICENSE`-Datei für weitere Details.