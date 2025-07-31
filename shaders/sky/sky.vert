#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    
} cameraUbo;


layout(push_constant) uniform SkyPushConstant {
    mat4 model;
} skyPc;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;

    mat4 viewNoTranslation = mat4(mat3(cameraUbo.view));
    
    vec4 worldPos = skyPc.model * vec4(inPosition, 1.0);
    vec4 pos = cameraUbo.proj * viewNoTranslation * worldPos;

    gl_Position = pos.xyww;
}