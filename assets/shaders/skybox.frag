#version 450

layout(location = 0) in vec3 inLocalPos;

layout(set = 1, binding = 0) uniform samplerCube prefilteredMap; // specularIBL;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap; // diffuseIBL;
layout(set = 1, binding = 2) uniform sampler2D brdfLUT; // Not used here

layout(location = 0) out vec4 outColor;

void main() {
    vec3 envColor = textureLod(prefilteredMap, inLocalPos, 0.5).rgb;
    //vec3 envColor = texture(irradianceMap, inLocalPos).rgb;
    
    // Apply tone mapping and gamma correction
    //envColor = envColor / (envColor + vec3(1.0));
    //envColor = pow(envColor, vec3(1.0/2.2));
    
    outColor = vec4(envColor, 1.0);
}