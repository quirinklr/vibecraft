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
    uint flags;
} cameraUbo;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;

    
    mat4 viewNoTranslation = mat4(mat3(cameraUbo.view));
    vec4 pos = cameraUbo.proj * viewNoTranslation * vec4(inPosition, 1.0);

    
    gl_Position = pos.xyww;
}