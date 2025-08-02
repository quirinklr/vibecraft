#version 450

layout(location = 0) in vec3 inPosition;

layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 skyColor;
    float time;
    int isUnderwater;
} cameraUbo;


layout(push_constant) uniform PushConstantData {
    mat4 model;
} pc;

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition, 1.0);
}