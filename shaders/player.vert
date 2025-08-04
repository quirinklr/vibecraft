#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

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

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}