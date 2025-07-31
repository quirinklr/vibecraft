#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord; 


layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 skyColor;
    float time;
    int isUnderwater;
} cameraUbo;

layout(push_constant) uniform SkyPushConstant {
    mat4 model;
    int is_sun;
} skyPc;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = cameraUbo.proj * cameraUbo.view * skyPc.model * vec4(inPosition, 1.0);
}