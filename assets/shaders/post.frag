#version 450

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform sampler2D floatColor2;

// Post-processing options uniform buffer
layout (set = 0, binding = 1) uniform PostProcessingOptions {
    // Tone mapping options
    int toneMappingType;        // 0=None, 1=Reinhard, 2=ACES, 3=Uncharted2, 4=GT, 5=Lottes, 6=Exponential, 7=ReinhardExtended, 8=Luminance, 9=Hable
    float exposure;             // HDR exposure adjustment
    float gamma;                // Gamma correction value
    float maxWhite;             // For extended Reinhard tone mapping
    
    // Color grading
    float contrast;             // Contrast adjustment
    float brightness;           // Brightness adjustment  
    float saturation;           // Color saturation
    float vibrance;             // Vibrance (smart saturation)
    
    // Effects
    float vignetteStrength;     // Vignette effect strength
    float vignetteRadius;       // Vignette radius
    float filmGrainStrength;    // Film grain noise strength
    float chromaticAberration;  // 0.0-1.0: Chromatic aberration, >1.0: FXAA strength
    
    // Debug and visualization
    int debugMode;              // 0=Off, 1=Show tone mapping comparison, 2=Show color channels
    float debugSplit;           // Split position for comparison (0.0-1.0)
    int showOnlyChannel;        // 0=All, 1=Red, 2=Green, 3=Blue, 4=Alpha, 5=Luminance
    float padding1;
} postOptions;

layout (location = 0) out vec4 outFragColor;

// ===== TONE MAPPING FUNCTIONS =====

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

vec3 uncharted2ToneMapping(vec3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;
    
    vec3 curr = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    vec3 whiteScale = ((vec3(W) * (A * vec3(W) + C * B) + D * E) / (vec3(W) * (A * vec3(W) + B) + D * F)) - E / F;
    return curr / whiteScale;
}

vec3 gtToneMapping(vec3 color) {
    const float P = 1.0;
    const float a = 1.0;
    const float m = 0.22;
    const float l = 0.4;
    const float c = 1.33;
    const float b = 0.0;
    
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    vec3 w0 = vec3(1.0) - smoothstep(0.0, m, color);
    vec3 w2 = step(m + l0, color);
    vec3 w1 = vec3(1.0) - w0 - w2;

    vec3 T = m * pow(color / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (color - S0));
    vec3 L = m + a * (color - m);

    return T * w0 + L * w1 + S * w2;
}

vec3 lottesToneMapping(vec3 color) {
    const vec3 a = vec3(1.6);
    const vec3 d = vec3(0.977);
    const vec3 hdrMax = vec3(8.0);
    const vec3 midIn = vec3(0.18);
    const vec3 midOut = vec3(0.267);
    
    const vec3 b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) / 
                   ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    const vec3 c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) / 
                   ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    
    return pow(color, a) / (pow(color, a * d) * b + c);
}

vec3 exponentialToneMapping(vec3 color) {
    return vec3(1.0) - exp(-color * 1.0);
}

vec3 reinhardExtendedToneMapping(vec3 color, float maxWhite) {
    vec3 numerator = color * (1.0 + (color / (maxWhite * maxWhite)));
    return numerator / (1.0 + color);
}

vec3 luminanceToneMapping(vec3 color) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return color / (1.0 + luminance);
}

vec3 hable(vec3 color) {
    const float A = 0.22;
    const float B = 0.30;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.01;
    const float F = 0.30;
    
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

vec3 hableToneMapping(vec3 color) {
    const float exposureBias = 2.0;
    vec3 x = hable(exposureBias * color);
    vec3 whiteScale = 1.0 / hable(vec3(11.2));
    return x * whiteScale;
}

// Apply tone mapping based on selected type
vec3 applyToneMapping(vec3 color) {
    switch(postOptions.toneMappingType) {
        case 1: return reinhardToneMapping(color);
        case 2: return acesToneMapping(color);
        case 3: return uncharted2ToneMapping(color);
        case 4: return gtToneMapping(color);
        case 5: return lottesToneMapping(color);
        case 6: return exponentialToneMapping(color);
        case 7: return reinhardExtendedToneMapping(color, postOptions.maxWhite);
        case 8: return luminanceToneMapping(color);
        case 9: return hableToneMapping(color);
        default: return color; // No tone mapping
    }
}

// ===== COLOR GRADING FUNCTIONS =====

vec3 adjustContrast(vec3 color, float contrast) {
    return (color - 0.5) * contrast + 0.5;
}

vec3 adjustSaturation(vec3 color, float saturation) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luminance), color, saturation);
}

vec3 adjustVibrance(vec3 color, float vibrance) {
    float maxComponent = max(max(color.r, color.g), color.b);
    float minComponent = min(min(color.r, color.g), color.b);
    float satLevel = maxComponent - minComponent;
    
    float vibranceAdjust = 1.0 + vibrance * (1.0 - satLevel);
    return mix(vec3(dot(color, vec3(0.2126, 0.7152, 0.0722))), color, vibranceAdjust);
}

// ===== EFFECT FUNCTIONS =====

vec3 applyVignette(vec3 color, vec2 uv) {
    if (postOptions.vignetteStrength <= 0.0) return color;
    
    vec2 center = vec2(0.5, 0.5);
    float distance = length(uv - center);
    float vignette = smoothstep(postOptions.vignetteRadius, postOptions.vignetteRadius - postOptions.vignetteStrength, distance);
    return color * vignette;
}

vec3 addFilmGrain(vec3 color, vec2 uv) {
    if (postOptions.filmGrainStrength <= 0.0) return color;
    
    // Simple noise function
    float noise = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    noise = (noise - 0.5) * postOptions.filmGrainStrength;
    return color + noise;
}

// ===== ADVANCED FXAA IMPLEMENTATION =====

float fxaaLuma(vec3 rgb) {
    return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}

// Enhanced FXAA with multiple quality levels and better edge detection
vec3 fxaaAdvanced(vec2 uv, float fxaaStrength) {
    vec2 texelSize = 1.0 / textureSize(floatColor2, 0);
    
    // Determine quality level from strength (encoded in fractional part)
    float baseStrength = floor(fxaaStrength * 10.0) / 10.0;
    float qualityLevel = (fxaaStrength - baseStrength) * 10.0;
    
    int sampleCount = int(mix(4.0, 12.0, qualityLevel)); // 4-12 samples based on quality
    bool useExtendedSampling = qualityLevel > 0.5;
    
    // Sample center and immediate neighbors
    vec3 rgbM = texture(floatColor2, uv).rgb;
    vec3 rgbN = texture(floatColor2, uv + vec2(0.0, -texelSize.y)).rgb;
    vec3 rgbS = texture(floatColor2, uv + vec2(0.0, texelSize.y)).rgb;
    vec3 rgbW = texture(floatColor2, uv + vec2(-texelSize.x, 0.0)).rgb;
    vec3 rgbE = texture(floatColor2, uv + vec2(texelSize.x, 0.0)).rgb;
    
    // Calculate luminance
    float lumaM = fxaaLuma(rgbM);
    float lumaN = fxaaLuma(rgbN);
    float lumaS = fxaaLuma(rgbS);
    float lumaW = fxaaLuma(rgbW);
    float lumaE = fxaaLuma(rgbE);
    
    // Find min/max luminance
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;
    
    // Adaptive quality settings based on strength
    float edgeThreshold = mix(0.2, 0.125, baseStrength);        // More sensitive at higher quality
    float edgeThresholdMin = mix(0.1, 0.0625, baseStrength);    // Minimum threshold
    float subpixelQuality = mix(0.5, 0.85, baseStrength);       // Blend amount
    
    // Skip FXAA if contrast is too low
    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold)) {
        return rgbM;
    }
    
    // Extended sampling for higher quality
    vec3 rgbNW = vec3(0.0), rgbNE = vec3(0.0), rgbSW = vec3(0.0), rgbSE = vec3(0.0);
    float lumaNW = 0.0, lumaNE = 0.0, lumaSW = 0.0, lumaSE = 0.0;
    
    if (useExtendedSampling) {
        rgbNW = texture(floatColor2, uv + vec2(-texelSize.x, -texelSize.y)).rgb;
        rgbNE = texture(floatColor2, uv + vec2(texelSize.x, -texelSize.y)).rgb;
        rgbSW = texture(floatColor2, uv + vec2(-texelSize.x, texelSize.y)).rgb;
        rgbSE = texture(floatColor2, uv + vec2(texelSize.x, texelSize.y)).rgb;
        
        lumaNW = fxaaLuma(rgbNW);
        lumaNE = fxaaLuma(rgbNE);
        lumaSW = fxaaLuma(rgbSW);
        lumaSE = fxaaLuma(rgbSE);
    }
    
    // Enhanced edge detection
    float edgeHorz = abs(lumaN + lumaS - 2.0 * lumaM) * 2.0 + 
                   abs(lumaN - lumaM) + abs(lumaS - lumaM);
    float edgeVert = abs(lumaW + lumaE - 2.0 * lumaM) * 2.0 + 
                   abs(lumaW - lumaM) + abs(lumaE - lumaM);
    
    if (useExtendedSampling) {
        edgeHorz += abs(lumaNW + lumaSW - 2.0 * lumaW) + abs(lumaNE + lumaSE - 2.0 * lumaE);
        edgeVert += abs(lumaNW + lumaNE - 2.0 * lumaN) + abs(lumaSW + lumaSE - 2.0 * lumaS);
    }
    
    bool isHorizontal = edgeHorz >= edgeVert;
    
    // Multi-sample blur for higher quality
    vec3 blendColor = rgbM;
    if (sampleCount <= 4) {
        // Simple 2-tap blur
        vec2 blendOffset = isHorizontal ? vec2(0.0, texelSize.y) : vec2(texelSize.x, 0.0);
        blendColor = (texture(floatColor2, uv + blendOffset).rgb + 
                      texture(floatColor2, uv - blendOffset).rgb) * 0.5;
    } else {
        // Multi-tap blur for higher quality
        vec2 blendDir = isHorizontal ? vec2(0.0, texelSize.y) : vec2(texelSize.x, 0.0);
        
        // 3-tap blur
        if (sampleCount <= 8) {
            blendColor = (texture(floatColor2, uv + blendDir).rgb + 
                         texture(floatColor2, uv).rgb +
                         texture(floatColor2, uv - blendDir).rgb) / 3.0;
        } else {
            // 5-tap blur for maximum quality
            blendColor = (texture(floatColor2, uv + blendDir * 2.0).rgb +
                         texture(floatColor2, uv + blendDir).rgb + 
                         texture(floatColor2, uv).rgb +
                         texture(floatColor2, uv - blendDir).rgb +
                         texture(floatColor2, uv - blendDir * 2.0).rgb) / 5.0;
        }
    }
    
    // Adaptive blend based on edge strength
    float edgeStrength = lumaRange / lumaMax;
    float adaptiveBlend = subpixelQuality * smoothstep(0.0, 1.0, edgeStrength);
    
    return mix(rgbM, blendColor, adaptiveBlend);
}

vec3 applyChromaticAberrationOrFXAA(vec2 uv) {
    if (postOptions.chromaticAberration > 1.0) {
        // FXAA mode: chromaticAberration > 1.0 acts as FXAA strength with quality encoding
        float fxaaStrength = postOptions.chromaticAberration - 1.0; // 1.1 = 0.1 strength, 2.0 = 1.0 strength
        return fxaaAdvanced(uv, clamp(fxaaStrength, 0.0, 1.0));
    } else if (postOptions.chromaticAberration > 0.0) {
        // Chromatic aberration mode: 0.0-1.0 range
        vec2 center = vec2(0.5, 0.5);
        vec2 offset = (uv - center) * postOptions.chromaticAberration * 0.01;
        
        float r = texture(floatColor2, uv + offset).r;
        float g = texture(floatColor2, uv).g;
        float b = texture(floatColor2, uv - offset).b;
        
        return vec3(r, g, b);
    } else {
        // No effect
        return texture(floatColor2, uv).rgb;
    }
}

// ===== DEBUG FUNCTIONS =====

vec3 showColorChannel(vec3 color) {
    switch(postOptions.showOnlyChannel) {
        case 1: return vec3(color.r, 0.0, 0.0); // Red only
        case 2: return vec3(0.0, color.g, 0.0); // Green only
        case 3: return vec3(0.0, 0.0, color.b); // Blue only
        case 4: return vec3(1.0); // Alpha channel not available for vec3, show white
        case 5: return vec3(dot(color, vec3(0.2126, 0.7152, 0.0722))); // Luminance
        default: return color; // All channels
    }
}

vec3 toneMappingComparison(vec3 originalColor, vec2 uv) {
    // Show different tone mapping methods side by side
    float sections = 9.0;
    float sectionWidth = 1.0 / sections;
    int section = int(uv.x / sectionWidth);
    
    vec3 toneMapped;
    switch(section) {
        case 0: toneMapped = originalColor; break; // No tone mapping
        case 1: toneMapped = reinhardToneMapping(originalColor); break;
        case 2: toneMapped = acesToneMapping(originalColor); break;
        case 3: toneMapped = uncharted2ToneMapping(originalColor); break;
        case 4: toneMapped = gtToneMapping(originalColor); break;
        case 5: toneMapped = lottesToneMapping(originalColor); break;
        case 6: toneMapped = exponentialToneMapping(originalColor); break;
        case 7: toneMapped = reinhardExtendedToneMapping(originalColor, postOptions.maxWhite); break;
        case 8: toneMapped = hableToneMapping(originalColor); break;
        default: toneMapped = originalColor; break;
    }
    
    // Add labels (simple color coding)
    if (uv.y < 0.05) {
        vec3 labelColors[9] = vec3[](
            vec3(1.0, 1.0, 1.0), // None - White
            vec3(1.0, 0.0, 0.0), // Reinhard - Red
            vec3(0.0, 1.0, 0.0), // ACES - Green
            vec3(0.0, 0.0, 1.0), // Uncharted2 - Blue
            vec3(1.0, 1.0, 0.0), // GT - Yellow
            vec3(1.0, 0.0, 1.0), // Lottes - Magenta
            vec3(0.0, 1.0, 1.0), // Exponential - Cyan
            vec3(1.0, 0.5, 0.0), // ReinhardExt - Orange
            vec3(0.5, 0.0, 1.0)  // Hable - Purple
        );
        return labelColors[section];
    }
    
    return toneMapped;
}

void main() {
    vec2 uv = inTexCoord;
    
    // Sample the original HDR color with optional chromatic aberration OR FXAA
    vec3 originalColor = applyChromaticAberrationOrFXAA(uv);
    
    // Apply exposure
    vec3 color = originalColor * postOptions.exposure;
    
    // Debug mode: tone mapping comparison
    if (postOptions.debugMode == 1) {
        color = toneMappingComparison(color, uv);
    } else {
        // Apply tone mapping
        color = applyToneMapping(color);
    }
    
    // Color grading
    color = adjustContrast(color, postOptions.contrast);
    color = color + postOptions.brightness; // Brightness adjustment
    color = adjustSaturation(color, postOptions.saturation);
    color = adjustVibrance(color, postOptions.vibrance);
    
    // Effects
    color = applyVignette(color, uv);
    color = addFilmGrain(color, uv);
    
    // Debug: show individual color channels
    if (postOptions.debugMode == 2) {
        color = showColorChannel(color);
    }
    
    // Gamma correction
    color = pow(max(color, vec3(0.0)), vec3(1.0 / postOptions.gamma));
    
    // Debug split comparison
    if (postOptions.debugMode == 3 && uv.x > postOptions.debugSplit) {
        // Right side: processed
        outFragColor = vec4(color, 1.0);
    } else if (postOptions.debugMode == 3) {
        // Left side: original (with basic exposure and gamma)
        vec3 simple = pow(originalColor * postOptions.exposure, vec3(1.0 / postOptions.gamma));
        outFragColor = vec4(simple, 1.0);
    } else {
        outFragColor = vec4(color, 1.0);
    }
    
    // Add a thin line at the split position for debug mode 3
    if (postOptions.debugMode == 3 && abs(uv.x - postOptions.debugSplit) < 0.002) {
        outFragColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow line
    }
}