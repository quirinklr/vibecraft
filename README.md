# Vibecraft Engine - A C++ Voxel Project with Vulkan

This project is a voxel engine built from scratch in C++20, inspired by Minecraft. The focus is on a high-performance, modern, and clean architecture that fully leverages the capabilities of the Vulkan API to simulate an interactive, procedurally generated world.

## 🚀 Key Features

*   **Highly Efficient Rendering Pipeline with Vulkan:** Utilizes modern Vulkan concepts for cross-platform, high-performance graphics. The architecture is cleanly separated into logical components for device, swap chain, and command management.

*   **Fully Asynchronous & Prioritized Chunk System:**
    *   **Intelligent, Stutter-Free Loading:** The world is loaded and meshed based on player proximity—from the center outwards. This ensures a seamless gameplay experience without visible gaps during fast movement.
    *   **Efficient Thread Pool:** A `std::jthread`-based thread pool handles computationally expensive tasks like terrain generation, meshing, and GPU data preparation in the background, without impacting the main thread's frame rate.

*   **Interactive World & Player Controller:**
    *   **Physics-Based Character:** Instead of a simple fly-through camera, you control a character with gravity, jumping mechanics, and AABB-based collision detection with the world.
    *   **Block Interaction:** Destroy blocks in real-time using ray-casting. Thanks to a "swap-on-ready" mechanism, chunks are updated **flicker-free** as soon as the new mesh is ready on the GPU.
    *   **Extensible Entity System:** A clean `Entity` base class allows for the easy addition of new creatures (zombies, animals, etc.) with their own logic in the future.

*   **Optimized Voxel Rendering:**
    *   **Greedy Meshing:** Drastically reduces the vertex count of chunks by merging adjacent, identical block faces into large polygons.
    *   **Frustum Culling:** Significantly reduces GPU load by only rendering chunks that are actually within the camera's view frustum.

*   **Modular World Generation:**
    *   **Layered Noise:** Uses `FastNoiseLite` (OpenSimplex2/Perlin) to create varied terrain through multiple layered noise maps for continents, erosion, and caves.
    *   **Biome System:** A clean, extensible system based on temperature maps generates different biomes like plains, deserts, and oceans.

## 🛠️ Technologies Used

*   **Language:** C++20
*   **Build System:** CMake
*   **Graphics API:** [Vulkan](https://www.vulkan.org/)
*   **Memory Management:** [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
*   **Windowing:** [GLFW](https://www.glfw.org/)
*   **Math:** [GLM](https://github.com/g-truc/glm)
*   **Noise Generation:** [FastNoiseLite](https://github.com/Auburn/FastNoiseLite)
*   **Image Loading:** [stb_image](https://github.com/nothings/stb)

## ⚙️ Running & Building

### 🎮 For Players (Release Version)

1.  Go to the project's [**Releases page**](https://github.com/quirinklr/minecraft-vibe/releases).
2.  Download the `.zip` file of the latest release (e.g., `Vibecraft-v1.0.0-win64.zip`).
3.  **IMPORTANT:** Install the [**Microsoft Visual C++ Redistributable (x64)**](https://aka.ms/vs/17/release/vc_redist.x64.exe). This is a one-time setup.
4.  Unzip the file anywhere.
5.  Run `Vibecraft.exe`.

### 👨‍💻 For Developers (From Source)

#### Prerequisites

*   A C++20 compliant compiler (e.g., MSVC, GCC, Clang)
*   [CMake](https://cmake.org/download/) (Version 3.10+)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

#### Steps

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/quirinklr/minecraft-vibe.git
    cd minecraft-vibe
    ```

2.  **Configure the project with CMake:**
    ```bash
    cmake -B build
    ```

3.  **Build the project (Release mode is recommended):**
    ```bash
    cmake --build build --config Release
    ```

4.  **Run the application:**
    You will find the executable in the `build/Release` directory.
    ```bash
    ./build/Release/Vibecraft.exe
    ```

## 📄 License

This project is licensed under the MIT License. See the `LICENSE` file for more details.