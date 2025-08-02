#version 450
layout(location = 0) in  vec3 inPosition;
layout(location = 1) in  vec3 inTileOrigin;          
layout(location = 2) in  vec2 inBlockUV;

layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 skyColor;
    float time;
    int isUnderwater;
} cameraUbo;

layout(std140, set = 0, binding = 6) readonly buffer ModelMatrixSSBO {
    mat4 models[];
} modelData;

layout(location = 2) out vec3 fragWorldPos;
layout(location = 0) flat out vec2 tileOrigin;       
layout(location = 1)      out vec2 localUV;

void main() {
    mat4 modelMatrix = modelData.models[gl_InstanceIndex];

    gl_Position = cameraUbo.proj * cameraUbo.view * modelMatrix * vec4(inPosition, 1.0);
    tileOrigin = inTileOrigin.xy;
    localUV    = inBlockUV;
    fragWorldPos = (modelMatrix * vec4(inPosition, 1.0)).xyz;
}