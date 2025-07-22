# Minecraft Clone in C++ mit Vulkan

Dieses Projekt ist ein Versuch, das beliebte Spiel Minecraft von Grund auf in C++ mit der Vulkan-Grafik-API neu zu erstellen.

## 🚀 Features

*   **Moderne Grafik-API:** Nutzt Vulkan für hochleistungsfähige, plattformübergreifende Grafik.
*   **Grundlegende Spiel-Engine:** Beinhaltet eine einfache Engine-Architektur mit einer Game-Loop, Fensterverwaltung und Kamera-Steuerung.
*   **Block-Rendering:** Kann Minecraft-Blocktexturen laden und in einer 3D-Welt darstellen.

*(Hier könntest du später weitere implementierte Features wie Spieler-Bewegung, Block-Platzierung, Welt-Generierung usw. hinzufügen.)*

## 🛠️ Verwendete Technologien

*   **Sprache:** C++
*   **Build-System:** CMake
*   **Grafik-API:** [Vulkan](https.://www.vulkan.org/)
*   **Fensterverwaltung:** [GLFW](https://www.glfw.org/)
*   **Bild-Laden:** [stb_image](https://github.com/nothings/stb)

## ⚙️ Bauen und Ausführen

### Voraussetzungen

*   Ein C++ Compiler, der C++17 unterstützt
*   [CMake](https://cmake.org/download/)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

### Schritte

1.  **Klone das Repository:**
    ```bash
    git clone <repository-url>
    cd Minecraft
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
    Die ausführbare Datei findest du im `build/Debug` (oder einem ähnlichen) Verzeichnis.
    ```bash
    ./build/Debug/Minecraft
    ```

## 🖼️ Screenshots

*(Hier wäre ein guter Platz, um Screenshots oder GIFs deines Projekts hinzuzufügen!)*

## 📄 Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe die `LICENSE`-Datei für weitere Details. *(Du müsstest noch eine LICENSE-Datei hinzufügen)*
