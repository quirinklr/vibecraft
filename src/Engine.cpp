#include "Engine.h"

#include <iostream>

Engine::Engine() {
    // Konstruktor bleibt leer, da Initialisierung in den Membern erfolgt
}

Engine::~Engine() {
    // Destruktor bleibt leer, da Aufräumen in den Member-Destruktoren erfolgt
}

void Engine::run() {
    while (!m_Window.shouldClose()) {
        glfwPollEvents();
        m_Renderer.drawFrame();
    }
}