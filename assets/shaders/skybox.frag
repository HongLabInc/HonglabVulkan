#version 450

layout(location = 0) in vec3 inLocalPos;

layout(set = 0, binding = 1) uniform SkyOptionsUBO {
    float exposure;
    float environmentIntensity;
    float roughnessLevel;
    uint useIrradianceMap;
    uint showMipLevels;
    uint showCubeFaces;
    float padding1;
    float padding2;
} skyOptions;

layout(set = 1, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform sampler2D brdfLUT;

layout(location = 0) out vec4 outColor;

// Debug visualization functions
vec3 getMipLevelColor(float mipLevel) {
    // Color code different mip levels
    if (mipLevel < 1.0) return mix(vec3(1,0,0), vec3(1,1,0), mipLevel);
    else if (mipLevel < 2.0) return mix(vec3(1,1,0), vec3(0,1,0), mipLevel - 1.0);
    else if (mipLevel < 3.0) return mix(vec3(0,1,0), vec3(0,1,1), mipLevel - 2.0);
    else if (mipLevel < 4.0) return mix(vec3(0,1,1), vec3(0,0,1), mipLevel - 3.0);
    else return mix(vec3(0,0,1), vec3(1,0,1), clamp(mipLevel - 4.0, 0.0, 1.0));
}

vec3 getCubeFaceColor(vec3 dir) {
    vec3 absDir = abs(dir);
    if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
        return dir.x > 0.0 ? vec3(1,0,0) : vec3(0,1,1); // +X: red, -X: cyan
    } else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
        return dir.y > 0.0 ? vec3(0,1,0) : vec3(1,0,1); // +Y: green, -Y: magenta
    } else {
        return dir.z > 0.0 ? vec3(0,0,1) : vec3(1,1,0); // +Z: blue, -Z: yellow
    }
}

void main() {
    vec3 envColor;
    
    // Sample environment map based on mode
    if (skyOptions.useIrradianceMap != 0) {
        envColor = texture(irradianceMap, inLocalPos).rgb;
    } else {
        envColor = textureLod(prefilteredMap, inLocalPos, skyOptions.roughnessLevel).rgb;
    }
    
    // Apply HDR controls (skybox-specific)
    envColor *= skyOptions.environmentIntensity;
    envColor *= skyOptions.exposure;
    
    // Debug visualizations
    if (skyOptions.showMipLevels != 0) {
        envColor = mix(envColor, getMipLevelColor(skyOptions.roughnessLevel), 0.5);
    }
    
    if (skyOptions.showCubeFaces != 0) {
        envColor = mix(envColor, getCubeFaceColor(inLocalPos), 0.3);
    }
    
    // Output raw HDR values (no tone mapping - that's for post-processing)
    outColor = vec4(envColor, 1.0);
}