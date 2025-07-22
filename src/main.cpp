//  main.cpp
#include <iostream>
#include <exception>
#include "Engine.h"

int main()
{
    glfwSetErrorCallback([](int code, const char *desc)
                         { std::cerr << "GLFW‑Error (" << code << "): " << desc << std::endl; });

    try
    {
        std::cout << "Starte Engine ...\n";
        Engine engine;
        engine.run();
        std::cout << "run() beendet\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n============\nEXCEPTION: " << e.what()
                  << "\n============\n";
        std::cin.get(); // hier pausieren
    }

    std::cout << "Programmende – <Return> druecken …";
    std::cin.get();
    return 0;
}
