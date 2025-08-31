#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT vec3 payloadColor;

void main() {
  // For shadow any-hit pass we won't use this file when TerminateOnFirstHit is used,
  // keep as placeholder for group completeness.
  payloadColor.r = 1.0;
}

