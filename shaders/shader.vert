#version 450
layout(location = 0) in  vec3 inPosition;
layout(location = 1) in  vec3 inTileOrigin;          
layout(location = 2) in  vec2 inBlockUV;             

layout(set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
} cameraUbo;

layout(push_constant) uniform PushConstantData { mat4 model; } pc;

layout(location = 0) flat out vec2 tileOrigin;       
layout(location = 1)      out vec2 localUV;

void main() {
    gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition,1);
    tileOrigin = inTileOrigin.xy;
    localUV    = inBlockUV;
}
