#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT vec3 payloadColor;

void main() {
  // For shadow rays, miss = light visible
  payloadColor.r = 0.0;
}

