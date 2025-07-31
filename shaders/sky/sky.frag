#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(push_constant) uniform SkyPushConstant {
    mat4 model;
    int is_sun;
} skyPc;

layout(std140, set = 0, binding = 3) uniform ULight {
    vec3 lightDirection;
};

layout(set = 0, binding = 2) uniform sampler2D sunSampler;
layout(set = 0, binding = 4) uniform sampler2D moonSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 textureColor;
    
    
    if (skyPc.is_sun == 1) {
        textureColor = texture(sunSampler, fragTexCoord);
    } else {
        
        vec4 moonTex = texture(moonSampler, fragTexCoord);
        vec3 ambientGlow = vec3(0.6, 0.6, 0.7); 
        moonTex.rgb += ambientGlow;
        textureColor = moonTex;
    }

    
    if (textureColor.a < 0.1) {
        discard;
    }

    
    outColor = textureColor;
}