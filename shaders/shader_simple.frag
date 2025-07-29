#version 450
layout(location = 2) in float inAo;
layout(location = 0) out vec4 outColor;

void main(){
    float ao = pow(inAo / 3.0, 1.6);
    outColor = vec4(vec3(ao), 1.0);
}
