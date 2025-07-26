#include <iostream>
#include <exception>
#include "Engine.h"
#include "Block.h"

int main()
{
    glfwSetErrorCallback([](int code, const char *desc)
                         { std::cerr << "GLFW‑Error (" << code << "): " << desc << std::endl; });

    try
    {
        BlockDatabase::get().init();
        std::cout << "Starte Engine ...\n";
        Engine engine;
        engine.run();
        std::cout << "run() beendet\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n============\nEXCEPTION: " << e.what()
                  << "\n============\n";
        std::cin.get();
    }

    std::cout << "Programmende – <Return> druecken …";
    std::cin.get();
    return 0;
}