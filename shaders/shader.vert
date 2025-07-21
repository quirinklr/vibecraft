#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// NEU: Separates UBO nur für die Kamera
layout(set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
} cameraUbo;

// NEU: Push-Konstante für die Model-Matrix
layout(push_constant) uniform PushConstantData {
    mat4 model;
} pushConstant;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pushConstant.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}