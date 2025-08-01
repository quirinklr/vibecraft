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
    
    if (cameraUbo.isUnderwater == 1) {
        vec3 fogColor = vec3(0.1, 0.2, 0.3); 
        float fogDensity = 0.02; 

        float dist = distance(cameraUbo.cameraPos, fragWorldPos);
        float fogFactor = 1.0 / exp(dist * dist * fogDensity); 
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        outColor = mix(vec4(fogColor, 1.0), textureColor, fogFactor);

    } else {
        
        
        float sunUpFactor = smoothstep(-0.15, 0.1, uboLight.lightDirection.y);
        vec3 finalLight = mix(AMBIENT_NIGHT, SUNLIGHT, sunUpFactor);

        
        float computedShadow = texture(shadowMap, gl_FragCoord.xy / textureSize(shadowMap, 0)).r;
        float shadowFactor = mix(1.0, computedShadow, float((cameraUbo.flags & FLAG_SHADOWS) != 0));

        outColor.rgb = textureColor.rgb * finalLight * shadowFactor;
        outColor.a = textureColor.a;
    }
}