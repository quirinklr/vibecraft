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
vec3 tileOrigin;
} pc;

layout(location = 0) flat out vec2 fragTileOrigin;
layout(location = 1) out vec2 fragLocalUV;
layout(location = 2) out vec3 fragWorldPos;

void main() {
gl_Position = cameraUbo.proj * cameraUbo.view * pc.model * vec4(inPosition, 1.0);
fragTileOrigin = pc.tileOrigin.xy;
fragLocalUV = inTexCoord;
fragWorldPos = (pc.model * vec4(inPosition, 1.0)).xyz;
}