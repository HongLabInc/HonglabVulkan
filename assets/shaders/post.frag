#version 450

layout (location = 0) in vec2 inTexCoord;

layout (set = 0, binding = 0) uniform sampler2D hdrColorBuffer;

layout (location = 0) out vec4 outFragColor;

vec3 reinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 gammaCorrection(vec3 color, float gamma) {
    return pow(color, vec3(1.0 / gamma));
}


// ACES Filmic Tone Mapping (Industry Standard)
vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Uncharted 2 Filmic Tone Mapping
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

// GT (Gran Turismo) Tone Mapping
vec3 gtToneMapping(vec3 color) {
    const float P = 1.0;  // max display brightness
    const float a = 1.0;  // contrast
    const float m = 0.22; // linear section start
    const float l = 0.4;  // linear section length
    const float c = 1.33; // black
    const float b = 0.0;  // pedestal
    
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

// Lottes Tone Mapping
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

// Exponential Tone Mapping
vec3 exponentialToneMapping(vec3 color) {
    return vec3(1.0) - exp(-color * 1.0);
}

// Extended Reinhard
vec3 reinhardExtendedToneMapping(vec3 color, float maxWhite) {
    vec3 numerator = color * (1.0 + (color / (maxWhite * maxWhite)));
    return numerator / (1.0 + color);
}

// Simple Luminance-based Tone Mapping
vec3 luminanceToneMapping(vec3 color) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return color / (1.0 + luminance);
}

// Hable (Uncharted 2) Function
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

void main() 
{
    vec4 originalColor = texture(hdrColorBuffer, inTexCoord);
    vec3 color;

    /*if (inTexCoord.x < 0.1) {
        // Left half
        if (inTexCoord.y < 0.1) {
            // Left top half: tone mapping + gamma correction
            color = reinhardToneMapping(originalColor.rgb);
            color = gammaCorrection(color, 2.2);
        } else {
            // Left bottom half: original input
            color = originalColor.rgb;
        }
    } 
    else*/

    if(inTexCoord.y < 0.02){
        color = vec3(1.0, 1.0, 0.0); // Yellow color for the right top half
    }
    else
    {
        float exposure = 1.0; // Adjust this value to control brightness
        color = originalColor.rgb * exposure;
        //color = acesToneMapping(color.rgb);
        //color = reinhardToneMapping(originalColor.rgb);
        
    }

    // outFragColor = vec4(color, originalColor.a);
    color = pow(color, vec3(1.0/2.2));
    outFragColor = vec4(color, 1.0); // Set alpha to 1.0 for full opacity
}
