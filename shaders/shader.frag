#version 450

// Nimmt die Farbe vom Vertex Shader entgegen
layout(location = 0) in vec3 fragColor;

// Gibt die finale Farbe für den Pixel aus
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}