#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord; // NEU

layout(set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
} cameraUbo;

layout(push_constant) uniform PushConstantData {
    mat4 model;
} pushConstant;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord; // NEU

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pushConstant.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}