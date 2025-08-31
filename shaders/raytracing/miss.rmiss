#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload;

layout(set = 0, binding = 2) uniform sampler2D texSun;
layout(set = 0, binding = 3) uniform sampler2D texMoon;

layout(push_constant) uniform Push {
  vec3 camPos; float fovYTan;
  vec3 camRight; float pad0;
  vec3 camUp; float pad1;
  vec3 camForward; float pad2;
  vec3 lightDir; float pad3;
  vec4 shadowParams;
  vec4 renderParams;
} pc;

void buildBasis(in vec3 d, out vec3 t, out vec3 b) {
  vec3 up = abs(d.y) > 0.9 ? vec3(1,0,0) : vec3(0,1,0);
  t = normalize(cross(up, d));
  b = normalize(cross(d, t));
}

void main() {
  vec3 rd = normalize(gl_WorldRayDirectionEXT);
  vec3 S = -pc.lightDir; // scene_to_sun
  vec3 M = -S;           // scene_to_moon

  float sunAlt = S.y; // >0 day, <0 night
  float dayFactor = smoothstep(-0.05, 0.2, sunAlt);

  float t = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 daySkyTop = vec3(0.30, 0.55, 0.95);
  vec3 daySkyHzn = vec3(0.80, 0.90, 1.00);
  vec3 nightSkyTop = vec3(0.02, 0.04, 0.08);
  vec3 nightSkyHzn = vec3(0.07, 0.08, 0.12);
  vec3 daySky = mix(daySkyHzn, daySkyTop, t);
  vec3 nightSky = mix(nightSkyHzn, nightSkyTop, t);
  vec3 sky = mix(nightSky, daySky, dayFactor);

  float sunRadius = radians(3.0);
  float moonRadius = radians(3.0);

  float aSun = acos(clamp(dot(rd, S), -1.0, 1.0));
  if (aSun < sunRadius) {
    vec3 ts, bs; buildBasis(S, ts, bs);
    vec2 uv = vec2(dot(rd, ts), dot(rd, bs)) / tan(sunRadius) * 0.5 + 0.5;
    vec3 sunCol = texture(texSun, uv).rgb;
    sky = mix(sky, sunCol, 0.9);
    sky += sunCol * 0.3;
  }

  float aMoon = acos(clamp(dot(rd, M), -1.0, 1.0));
  if (aMoon < moonRadius) {
    vec3 tm, bm; buildBasis(M, tm, bm);
    vec2 uv = vec2(dot(rd, tm), dot(rd, bm)) / tan(moonRadius) * 0.5 + 0.5;
    vec3 moonCol = texture(texMoon, uv).rgb;
    sky = mix(sky, moonCol, 0.7 * (1.0 - dayFactor));
  }

  payload.rgb = sky;
}

