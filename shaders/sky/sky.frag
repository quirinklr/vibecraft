#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(std140, set = 0, binding = 3) uniform ULight {
    vec3 lightDirection;
    
};

layout(set = 0, binding = 2) uniform sampler2D sunSampler;
layout(set = 0, binding = 4) uniform sampler2D moonSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sunColor = texture(sunSampler, fragTexCoord);
    vec4 moonColor = texture(moonSampler, fragTexCoord);

    
    
    float sunFactor = smoothstep(-0.1, 0.1, lightDirection.y);

    vec4 finalColor = mix(moonColor, sunColor, sunFactor);

    
    if (finalColor.a < 0.1) {
        discard;
    }

    outColor = finalColor;
}