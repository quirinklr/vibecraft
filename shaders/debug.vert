#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
} cameraUbo;

layout(push_constant) uniform PushConstantData {
    mat4 model;
} pc;

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition, 1.0);
}