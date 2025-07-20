#include "Engine.h"
#include <iostream>
#include <stdexcept>

int main() {
    Engine engine;

    try {
        engine.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cin.get(); // Hält die Konsole bei einem Fehler offen
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}