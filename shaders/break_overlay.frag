#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in int fragStage;



layout(set = 0, binding = 2) uniform sampler2D destroySampler;

layout(location = 0) out vec4 outColor;

void main() {
    
    
    float stage_width = 1.0 / 10.0;
    
    
    
    float u_offset = float(fragStage) * stage_width;
    
    
    
    vec2 uv = vec2(fragTexCoord.x * stage_width + u_offset, fragTexCoord.y);
    
    
    vec4 texColor = texture(destroySampler, uv);

    
    if (texColor.a < 0.1) {
        discard;
    }
    
    outColor = texColor;
}