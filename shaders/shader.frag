#version 450
layout(location = 0) in  vec3 fragColor;
layout(location = 1) in  vec2 fragTexCoord;
layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

void main() {
    const float TILE = 1.0 / 16.0;

    vec2 tileUV  = fract(fragTexCoord / TILE);        
    vec2 tileIdx = floor(fragTexCoord / TILE) * TILE; 

    outColor = texture(texSampler, tileIdx + tileUV * TILE);
}
