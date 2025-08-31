#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload; // rgb + bounceDepth
layout(location = 1) rayPayloadEXT vec3 shadowPayload;  // reused for shadow rays
layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2) uniform sampler2D texSun;
layout(set = 0, binding = 3) uniform sampler2D texMoon;

// DDGI params + probe buffer (optional, may be unbound)
layout(set = 0, binding = 4) uniform DDGIUbo { int gsx; int gsy; int gsz; float spacing; float hysteresis; int raysPerProbe; int sliceAxis; int sliceIndex; float originX; float originY; float originZ; float rotation; } ddgi;
layout(set = 0, binding = 5) buffer DDGIProbes { float data[]; } ddgiBuf;

layout(push_constant) uniform Push {
  vec3 camPos; float fovYTan;
  vec3 camRight; float pad0;
  vec3 camUp; float pad1;
  vec3 camForward; float pad2;
  vec3 lightDir; float pad3;
  vec4 shadowParams;
  vec4 renderParams; // x=reflectionMode, y=nightBrightness, z=shadowMinVisibility
} pc;

vec3 cubeFaceNormal(uint prim) {
  uint face = prim / 2u;
  if (face == 0u) return vec3(0.0, 0.0, -1.0);
  if (face == 1u) return vec3(0.0, 0.0,  1.0);
  if (face == 2u) return vec3(-1.0, 0.0, 0.0);
  if (face == 3u) return vec3( 1.0, 0.0, 0.0);
  if (face == 4u) return vec3(0.0, 1.0, 0.0);
  return vec3(0.0, -1.0, 0.0);
}

void buildBasis(in vec3 d, out vec3 t, out vec3 b) {
  vec3 up = abs(d.y) > 0.9 ? vec3(1,0,0) : vec3(0,1,0);
  t = normalize(cross(up, d));
  b = normalize(cross(d, t));
}

vec3 schlickFresnel(vec3 F0, float cosTheta) { return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }

void main() {
  vec3 N;
  vec3 albedo;
  uint inst = gl_InstanceCustomIndexEXT;
  if (inst == 2u) {
    N = vec3(0.0, 1.0, 0.0);
    vec3 Pw = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    int cx = int(floor(Pw.x));
    int cz = int(floor(Pw.z));
    bool check = ((cx + cz) & 1) == 0;
    albedo = check ? vec3(0.8) : vec3(0.6);
  } else {
    N = cubeFaceNormal(gl_PrimitiveID);
    if (inst == 3u) albedo = vec3(0.85);
    else if (inst == 4u) albedo = vec3(0.95, 0.98, 1.0);
    else albedo = vec3(0.8, 0.2, 0.2);
  }
  N = normalize(N);
  vec3 V = normalize(-gl_WorldRayDirectionEXT);

  vec3 Lsun = normalize(-pc.lightDir);
  vec3 Lmoon = normalize(pc.lightDir);
  bool day = ((-pc.lightDir).y) > 0.0;
  vec3 L = day ? Lsun : Lmoon;
  float lightIntensity = day ? 1.0 : 0.35;
  vec3 P = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + N * 0.001;
  float dayFactor = smoothstep(0.0, 0.2, (-pc.lightDir).y);

  vec3 color = vec3(0.0);
  float bounce = payload.w;

  // Primary hit shading with optional shadows and reflections
  float NoL = max(dot(N, L), 0.0);
  float visibility = 1.0;
  if (NoL > 0.0) {
    int mode = int(pc.shadowParams.x + 0.5);
    if (mode == 0) {
      visibility = 1.0;
    } else {
      float ang = pc.shadowParams.y;
      int S = max(1, int(pc.shadowParams.z + 0.5));
      vec3 t, b; buildBasis(L, t, b);
      float occ = 0.0;
      for (int i = 0; i < S; ++i) {
        float u = (float(i) + 0.5) / float(S);
        float r = sqrt(u);
        float phi = 6.2831853 * u;
        float a = r * ang;
        float ca = cos(a), sa = sin(a);
        vec3 dir = normalize(L * ca + (t * cos(phi) + b * sin(phi)) * sa);
        shadowPayload = vec3(0.0);
        traceRayEXT(topLevelAS,
                    gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT,
                    0xFF,
                    1, 1, 1,
                    P, 0.001,
                    dir, 10000.0,
                    1);
        occ += (shadowPayload.r > 0.5) ? 1.0 : 0.0;
      }
      visibility = 1.0 - occ / float(S);
    }
  }
  visibility = mix(pc.renderParams.z, 1.0, visibility);
  vec3 ambient = mix(0.01, 0.05, dayFactor) * albedo;
  vec3 direct = lightIntensity * visibility * NoL * albedo;

  // Optional metal reflections
  vec3 refl = vec3(0.0);
  if (inst == 3u && pc.renderParams.x > 0.5) {
    vec3 Rn = normalize(reflect(-V, N));
    payload.w = 1.0; payload.rgb = vec3(0.0);
    traceRayEXT(topLevelAS,
                gl_RayFlagsOpaqueEXT,
                0xFF,
                0, 1, 0,
                P + N * 0.01, 0.001,
                Rn, 10000.0,
                0);
    refl = payload.rgb;
    vec3 F = schlickFresnel(albedo, max(dot(N, V), 0.0));
    color = refl * F + direct + ambient * 0.05;
  } else {
    color = ambient + direct;
  }

  // Simple DDGI sampling (if bound)
  if (ddgi.gsx > 0 && ddgi.gsy > 0 && ddgi.gsz > 0) {
    vec3 Pw2 = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 o = vec3(ddgi.originX, ddgi.originY, ddgi.originZ);
    vec3 rp = (Pw2 - o) / ddgi.spacing;
    vec3 fc = clamp(fract(rp), 0.0, 1.0);
    ivec3 b0 = ivec3(min(max(floor(rp), vec3(0.0)), vec3(ddgi.gsx-1, ddgi.gsy-1, ddgi.gsz-1)));
    float Y0 = 0.282095;
    float Y1 = 0.488603 * N.y;
    float Y2 = 0.488603 * N.z;
    float Y3 = 0.488603 * N.x;
    float Y4 = 1.092548 * N.x * N.y;
    float Y5 = 1.092548 * N.y * N.z;
    float Y6 = 0.315392 * (3.0 * N.z * N.z - 1.0);
    float Y7 = 1.092548 * N.x * N.z;
    float Y8 = 0.546274 * (N.x * N.x - N.y * N.y);
    int stride = 9*3 + 4;
    vec3 gi = vec3(0.0);
    for (int dz = 0; dz < 2; ++dz)
    for (int dy = 0; dy < 2; ++dy)
    for (int dx = 0; dx < 2; ++dx) {
      int ix = min(max(b0.x + dx, 0), ddgi.gsx-1);
      int iy = min(max(b0.y + dy, 0), ddgi.gsy-1);
      int iz = min(max(b0.z + dz, 0), ddgi.gsz-1);
      int pi = ix + iy*ddgi.gsx + iz*ddgi.gsx*ddgi.gsy;
      int base = pi * stride;
      vec3 c0 = vec3(ddgiBuf.data[base+0], ddgiBuf.data[base+1], ddgiBuf.data[base+2]);
      vec3 c1 = vec3(ddgiBuf.data[base+3], ddgiBuf.data[base+4], ddgiBuf.data[base+5]);
      vec3 c2 = vec3(ddgiBuf.data[base+6], ddgiBuf.data[base+7], ddgiBuf.data[base+8]);
      vec3 c3 = vec3(ddgiBuf.data[base+9], ddgiBuf.data[base+10], ddgiBuf.data[base+11]);
      vec3 c4 = vec3(ddgiBuf.data[base+12], ddgiBuf.data[base+13], ddgiBuf.data[base+14]);
      vec3 c5 = vec3(ddgiBuf.data[base+15], ddgiBuf.data[base+16], ddgiBuf.data[base+17]);
      vec3 c6 = vec3(ddgiBuf.data[base+18], ddgiBuf.data[base+19], ddgiBuf.data[base+20]);
      vec3 c7 = vec3(ddgiBuf.data[base+21], ddgiBuf.data[base+22], ddgiBuf.data[base+23]);
      vec3 c8 = vec3(ddgiBuf.data[base+24], ddgiBuf.data[base+25], ddgiBuf.data[base+26]);
      vec3 sh = c0 * Y0 + c1 * Y1 + c2 * Y2 + c3 * Y3 + c4 * Y4 + c5 * Y5 + c6 * Y6 + c7 * Y7 + c8 * Y8;
      float wx = dx==0 ? (1.0 - fc.x) : fc.x;
      float wy = dy==0 ? (1.0 - fc.y) : fc.y;
      float wz = dz==0 ? (1.0 - fc.z) : fc.z;
      gi += sh * (wx*wy*wz);
    }
    color += gi * albedo;
  }

  payload.rgb = color;
  payload.w = bounce;
}

