#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 underneathColor = subpassLoad(inputColor);
    outColor = vec4(1.0 - underneathColor.rgb, 1.0);
}