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
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;

const vec3 AMBIENT_NIGHT = vec3(0.05, 0.1, 0.15);
const vec3 SUNLIGHT = vec3(0.8, 0.9, 1.0);

void main() {
    if (cameraUbo.isUnderwater == 1 && fragWorldPos.y > cameraUbo.cameraPos.y) {
        discard;
    }

    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    
    vec3 fogColor = vec3(0.00, 0.08, 0.25);

    vec3 viewVec = normalize(cameraUbo.cameraPos - fragWorldPos);
    vec3 normalVec = vec3(0.0, 1.0, 0.0);
    float angleFactor = 1.0 - dot(viewVec, normalVec);
    angleFactor = pow(angleFactor, 2.0);
    
    float dist = distance(cameraUbo.cameraPos, fragWorldPos);
    float distanceFactor = 1.0 - exp(-dist * 0.1);
    
    float fogAmount = max(angleFactor, distanceFactor);
    float fogFactor = 1.0 - clamp(fogAmount, 0.0, 1.0); 
    
    float surfaceFogFactor = pow(fogFactor, 0.5); 
    
    
    
    float sunUpFactor = smoothstep(-0.15, 0.1, uboLight.lightDirection.y);
    vec3 blockLight = mix(AMBIENT_NIGHT, SUNLIGHT, sunUpFactor);

    vec3 finalRgb = mix(fogColor, textureColor.rgb * blockLight, surfaceFogFactor);
    float finalAlpha = mix(0.8, textureColor.a, fogFactor); 

    outColor = vec4(finalRgb, finalAlpha);
}