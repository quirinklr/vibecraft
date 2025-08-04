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
    float seaLevel;
} cameraUbo;


layout(std140, set = 0, binding = 3) uniform ULight {
    vec3 lightDirection;
} uboLight;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;

const vec3 AMBIENT_NIGHT = vec3(0.05, 0.1, 0.15);
const vec3 SUNLIGHT = vec3(0.8, 0.9, 1.0);
const vec3 FOG_COLOR = vec3(0.05, 0.18, 0.25);

void main() {
    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    
    
    float sunUpFactor = smoothstep(-0.15, 0.1, uboLight.lightDirection.y);
    vec3 blockLight = mix(AMBIENT_NIGHT, SUNLIGHT, sunUpFactor);
    vec3 litTextureColor = textureColor.rgb * blockLight;

    
    float fogFactor;

    if (cameraUbo.isUnderwater == 1) {
        
        float dist = distance(cameraUbo.cameraPos, fragWorldPos);
        float fogDensity = 0.15; 
        fogFactor = exp(-dist * fogDensity);

    } else {
        
        float waterDepth = cameraUbo.seaLevel - fragWorldPos.y;
        float fogDensity = 0.5;
        
        
        vec3 viewVec = normalize(cameraUbo.cameraPos - fragWorldPos);
        vec3 normalVec = vec3(0.0, 1.0, 0.0);
        float fresnel = 0.2 + 0.8 * pow(1.0 - dot(normalVec, viewVec), 5.0);
        
        
        fogFactor = exp(-waterDepth * fogDensity);
        fogFactor = mix(fogFactor, 0.0, fresnel * 0.5);
    }

    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 finalRgb = mix(FOG_COLOR, litTextureColor, fogFactor);
    
    float finalAlpha = mix(0.9, textureColor.a, fogFactor); 

    outColor = vec4(finalRgb, finalAlpha);
}