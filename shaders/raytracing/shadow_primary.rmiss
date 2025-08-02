#version 460
#extension GL_EXT_ray_tracing : require


layout(location = 0) rayPayloadInEXT float shadowResult;

void main()
{
  
  
  shadowResult = 1.0;
}