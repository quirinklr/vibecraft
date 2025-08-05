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

layout(push_constant) uniform PushConstants {
    mat4 model;
    int stage;
} pc;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) flat out int fragStage;

void main() {
    
    gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition, 1.0);
    
    
    gl_Position.z -= 0.0001 * gl_Position.w;

    
    fragTexCoord = inTexCoord;
    fragStage = pc.stage;
}