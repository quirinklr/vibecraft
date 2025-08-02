#version 450
layout(location = 0) flat in vec2 tileOrigin;
layout(location = 1)      in vec2 localUV;
layout(location = 2)      in vec3 fragWorldPos; 

layout(std140, set = 0, binding = 0) uniform CameraUbo {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 skyColor;
    float time;
    int isUnderwater;
    uint flags;
} cameraUbo;


layout(std140, set = 0, binding = 3) uniform ULight {
    vec3 lightDirection;
    
} uboLight;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 5) uniform sampler2D shadowMap;
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;
const uint FLAG_SHADOWS = 1;
const vec3 AMBIENT_NIGHT = vec3(0.1, 0.12, 0.2);
const vec3 SUNLIGHT = vec3(1.0, 1.0, 0.95);

void main() {
    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    
    if (textureColor.a < 0.1) {
        discard;
    }
    
    
    float computedShadow = texture(shadowMap, gl_FragCoord.xy / textureSize(shadowMap, 0)).r;
    float shadowFactor = mix(1.0, computedShadow, float((cameraUbo.flags & FLAG_SHADOWS) != 0));

    
    outColor = vec4(vec3(shadowFactor), 1.0); 

    
    
}