#version 450

layout(location = 0) in vec3 inLocalPos;

layout(set = 0, binding = 1) uniform SkyOptionsUBO {
    float exposure;
    float roughnessLevel;
    uint useIrradianceMap;
    float environmentIntensity;
    uint enableToneMapping;
    uint toneMappingMode;
    float gamma;
    float whitePoint;
    vec3 colorTint;
    float saturation;
    float contrast;
    float brightness;
    uint showMipLevels;
    uint showCubeFaces;
    float padding1;
    float padding2;
} skyOptions;

layout(set = 1, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform sampler2D brdfLUT;

layout(location = 0) out vec4 outColor;

// Tone mapping functions
vec3 reinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 filmicToneMapping(vec3 color) {
    color = max(vec3(0.0), color - vec3(0.004));
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
    return color;
}

// Color grading functions
vec3 adjustSaturation(vec3 color, float saturation) {
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luminance), color, saturation);
}

vec3 adjustContrast(vec3 color, float contrast) {
    return (color - 0.5) * contrast + 0.5;
}

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
    
    // Sample environment map
    if (skyOptions.useIrradianceMap != 0) {
        envColor = texture(irradianceMap, inLocalPos).rgb;
    } else {
        envColor = textureLod(prefilteredMap, inLocalPos, skyOptions.roughnessLevel).rgb;
    }
    
    // Apply environment intensity
    envColor *= skyOptions.environmentIntensity;
    
    // Apply exposure
    envColor *= skyOptions.exposure;
    
    // Debug visualizations
    if (skyOptions.showMipLevels != 0) {
        envColor = mix(envColor, getMipLevelColor(skyOptions.roughnessLevel), 0.5);
    }
    
    if (skyOptions.showCubeFaces != 0) {
        envColor = mix(envColor, getCubeFaceColor(inLocalPos), 0.3);
    }
    
    // Color grading
    envColor *= skyOptions.colorTint;
    envColor += vec3(skyOptions.brightness);
    envColor = adjustSaturation(envColor, skyOptions.saturation);
    envColor = adjustContrast(envColor, skyOptions.contrast);
    
    // Tone mapping
    if (skyOptions.enableToneMapping != 0) {
        if (skyOptions.toneMappingMode == 0) {
            envColor = reinhardToneMapping(envColor);
        } else if (skyOptions.toneMappingMode == 1) {
            envColor = acesToneMapping(envColor);
        } else if (skyOptions.toneMappingMode == 2) {
            envColor = filmicToneMapping(envColor);
        }
    }
    
    // Gamma correction
    envColor = pow(envColor, vec3(1.0 / skyOptions.gamma));
    
    outColor = vec4(envColor, 1.0);
}