#version 450
layout(location = 0) flat in vec2 tileOrigin;
layout(location = 1)      in vec2 localUV;
layout(location = 2)      in vec3 fragWorldPos; 

layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float time;
    int isUnderwater;
} cameraUbo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;

void main() {
    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    
    vec3 fogColor = vec3(0.1, 0.2, 0.3); 
    float fogDensity = 0.3; 

    
    float dist = distance(cameraUbo.cameraPos, fragWorldPos);
    float fogFactor = 1.0 / exp(dist * fogDensity); 
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    vec4 finalColor = mix(vec4(fogColor, textureColor.a), textureColor, fogFactor);

    outColor = finalColor;
}