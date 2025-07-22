#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord; // NEU

layout(set = 0, binding = 1) uniform sampler2D texSampler; // NEU

layout(location = 0) out vec4 outColor;

void main() {
    // Nur die Texturfarbe verwenden
    outColor = texture(texSampler, fragTexCoord);
}