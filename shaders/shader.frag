#version 450
layout(location = 0) flat in vec2 tileOrigin;
layout(location = 1)      in vec2 localUV;


layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;
const int WATER_TEXTURE_INDEX = 3; 

void main() {
    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    
    vec3 finalColor = textureColor.rgb;

    int texture_id = int(tileOrigin.y * 16.0) * 16 + int(tileOrigin.x * 16.0);

    float alpha = textureColor.a;
    if (texture_id == WATER_TEXTURE_INDEX) {
        alpha = 0.7;
    }

    outColor = vec4(finalColor, alpha);
}