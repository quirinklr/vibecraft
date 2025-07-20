#version 450
#extension GL_ARB_separate_shader_objects : enable

// Wir definieren die Vertex-Positionen direkt im Shader
// Dies ist eine einfache Methode für ein statisches Dreieck, um Vertex-Buffer zu vermeiden.
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

// Wir definieren auch die Farben direkt im Shader
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // Rot
    vec3(0.0, 1.0, 0.0), // Grün
    vec3(0.0, 0.0, 1.0)  // Blau
);

// Der Output des Vertex Shaders, der zum Fragment Shader geht
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex ist eine eingebaute Variable, die von 0 bis 2 zählt.
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.dd0);
    fragColor = colors[gl_VertexIndex];
}