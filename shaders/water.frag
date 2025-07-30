#version 450
layout(location = 0) flat in vec2 tileOrigin;
layout(location = 1)      in vec2 localUV;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

const float TILE_SIZE = 1.0 / 16.0;

void main() {
    vec2 uv = tileOrigin + fract(localUV) * TILE_SIZE;
    vec4 textureColor = texture(texSampler, uv);
    outColor = textureColor;
}