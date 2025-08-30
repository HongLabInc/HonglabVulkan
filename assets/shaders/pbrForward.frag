#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;
layout(location = 5) in vec3 fragCameraPos;
layout(location = 6) in vec4 fragPosLightSpace;

layout(push_constant) uniform PushConstants {
    mat4 model;
    float coeffs[16];
} pushConstants;

layout(set = 0, binding = 0) uniform SceneDataUBO {
    mat4 projection;
    mat4 view;
    vec3 cameraPos;
    float padding1;
    vec3 directionalLightDir;
    float padding2;
    vec3 directionalLightColor;
    float padding3;
    mat4 lightSpaceMatrix;
} sceneData;

layout(set = 0, binding = 1) uniform OptionsUBO {
    int textureOn;
    int shadowOn; 
    int discardOn;
    int animationOn;
    float ssaoRadius;
    float ssaoBias;
    int ssaoSampleCount;
    float ssaoPower;
} options;

// Material properties
layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 emissiveFactor;
    vec4 baseColorFactor;
    float roughnessFactor;
    float transparencyFactor;
    float discardAlpha;
    float metallicFactor;
    int baseColorTextureIndex;
    int emissiveTextureIndex;
    int normalTextureIndex;
    int opacityTextureIndex;
    int metallicRoughnessTextureIndex;
    int occlusionTextureIndex;
} material;

// Material textures
layout(set = 1, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 1, binding = 2) uniform sampler2D emissiveTexture;
layout(set = 1, binding = 3) uniform sampler2D normalTexture;
layout(set = 1, binding = 4) uniform sampler2D opacityTexture;
layout(set = 1, binding = 5) uniform sampler2D metallicRoughnessTexture;
layout(set = 1, binding = 6) uniform sampler2D occlusionTexture;

// IBL textures
layout(set = 2, binding = 0) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 1) uniform samplerCube irradianceMap;
layout(set = 2, binding = 2) uniform sampler2D brdfLUT;

// Shadow map (주의: 각 셋의 바인딩은 0에서 시작해야 함)
layout(set = 3, binding = 0) uniform sampler2DShadow shadowMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

float calculateShadow(vec4 fragPosLightSpace)
{
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    //projCoords = projCoords * 0.5 + 0.5;
    //projCoords.y = 1.0 - projCoords.y;
    
    // Check if fragment is outside shadow map bounds
//    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
//        projCoords.y < 0.0 || projCoords.y > 1.0) {
//        return 0.0; // Outside shadow map = no shadow
//    }

    if(projCoords.z <= -1.0 || projCoords.z >= 1.0)
        return 1.0;
    
    // Get depth from light's perspective
    //float currentDepth = projCoords.z;
    //float bias = 0.05; // Fixed bias value, can be adjusted for better shadow quality
    //float biasedDepth = currentDepth - bias * pushConstants.coeffs2[3];
    //float biasedDepth = currentDepth;
    
    // Poisson disk sample offsets for more natural distribution
    const vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.09418410, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    
    // projCoords.y = 1.0 - projCoords.y;

    // PCF with Poisson disk sampling
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float filterRadius = 2.0; // Adjust for shadow softness
    
    for(int i = 0; i < 16; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * filterRadius;
        shadow += texture(shadowMap, vec3(projCoords.xy + offset, projCoords.z));
    }
    shadow /= 16.0;

    // shadow = mix(0.3, 1.0, shadow);
    
    return shadow;
}

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

vec3 getIBLGGXFresnel(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight)
{
    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    float NdotV = clamp(dot(n, v),0.0, 1.0);
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(brdfLUT, brdfSamplePoint).rg;
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * (k_S * f_ab.x + f_ab.y);

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);

    return FssEss + FmsEms;
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float lod = roughness * float(MAX_REFLECTION_LOD - 1) + 0.5;
    vec3 reflection = normalize(reflect(v, n));
    return textureLod(prefilteredMap, reflection, lod).rgb;
}

vec3 BRDF_lambertian(vec3 diffuseColor)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (diffuseColor / PI);
}

vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH) 
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float F_Schlick(float f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float F_Schlick(float f0, float VdotH)
{
    float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 F_Schlick(vec3 f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

vec3 F_Schlick(vec3 f0, float VdotH)
{
    float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (PI * f * f);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_specularGGX(float alphaRoughness, float NdotL, float NdotV, float NdotH)
{
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return vec3(Vis * D);
}

void main() {
    float specularWeight = pushConstants.coeffs[0];
    float diffuseWeight = pushConstants.coeffs[1];
    float emissiveWeight = pushConstants.coeffs[2];
    float shadowOffset = pushConstants.coeffs[3];

    // Sample material properties
    vec4 baseColorRGBA = (options.textureOn != 0 && material.baseColorTextureIndex >= 0) ? texture(baseColorTexture, fragTexCoord) : vec4(1.0);

    if(material.opacityTextureIndex >= 0) {
        float opacity = texture(opacityTexture, fragTexCoord).r;
        if(options.discardOn != 0 && opacity < 0.08)
            discard;
    }

    vec3 baseColor = material.baseColorFactor.rgb * baseColorRGBA.rgb;
    float metallic = material.metallicFactor * pushConstants.coeffs[4];
    float roughness = material.roughnessFactor * pushConstants.coeffs[5];

    if(material.metallicRoughnessTextureIndex >= 0){
        vec3 metallicRoughness = texture(metallicRoughnessTexture, fragTexCoord).rgb;
        metallic *= metallicRoughness.b; // Blue channel
        roughness *= metallicRoughness.g; // Green channel
    }

    float ao = 1.0;
    if(material.occlusionTextureIndex >= 0) {
       ao = texture(occlusionTexture, fragTexCoord).r;
   }

    vec3 emissive = material.emissiveFactor.xyz * emissiveWeight;
    if(material.emissiveTextureIndex >= 0)
    {
        emissive *= texture(emissiveTexture, fragTexCoord).xyz;
    }

    vec3 V = normalize(fragCameraPos - fragPos);
   
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    mat3 TBN = mat3(T, B, N);
    if(material.normalTextureIndex >= 0) {
      vec3 tangentNormal = texture(normalTexture, fragTexCoord).xyz * 2.0 - 1.0;
      if (length(tangentNormal) > 0.5)
        N = normalize(TBN * tangentNormal);
   }

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float TdotV = clamp(dot(T, V), 0.0, 1.0);
    float BdotV = clamp(dot(B, V), 0.0, 1.0);
    
    roughness = clamp(roughness, 0.0, 1.0);
    metallic =  clamp(metallic, 0.0, 1.0);

    vec3 f_diffuse = texture(irradianceMap, N).rgb * baseColor * diffuseWeight;

    vec3 f_specular_metal = getIBLRadianceGGX(N, V, roughness);
    vec3 f_specular_dielectric = f_specular_metal;

    vec3 f_metal_fresnel_ibl = getIBLGGXFresnel(N, V, roughness, baseColor.rgb, 1.0);
    vec3 f_metal_brdf_ibl = f_metal_fresnel_ibl * f_specular_metal;
 
    vec3 f0_dielectric = vec3(0.04);
    vec3 f90 = vec3(1.0);
    vec3 f90_dielectric = f90;

    vec3 f_dielectric_fresnel_ibl = getIBLGGXFresnel(N, V, roughness, f0_dielectric, specularWeight);
    vec3 f_dielectric_brdf_ibl = mix(f_diffuse, f_specular_dielectric,  f_dielectric_fresnel_ibl);

    float shadowFactor = 1.0;
    
    if(options.shadowOn != 0) {
        shadowFactor = calculateShadow(fragPosLightSpace) + shadowOffset;
        shadowFactor = clamp(shadowFactor, 0.0, 1.0);
    }

    // Directional light
    vec3 l_light = vec3(0.0);
    {
        vec3 l = normalize(sceneData.directionalLightDir);
        vec3 h = normalize(l + V);
        float NdotL = clampedDot(N, l);
        float NdotV = clampedDot(N, V);
        float NdotH = clampedDot(N, h);
        float LdotH = clampedDot(l, h);
        float VdotH = clampedDot(V, h);

        vec3 dielectric_fresnel = F_Schlick(f0_dielectric * specularWeight, f90_dielectric, abs(VdotH));
        vec3 metal_fresnel = F_Schlick(baseColor.rgb, vec3(1.0), abs(VdotH));
        vec3 lightIntensity = vec3(1.0, 0.9, 0.7) * sceneData.directionalLightColor;
        vec3 l_diffuse = lightIntensity * NdotL * BRDF_lambertian(baseColor.rgb);
        vec3 l_specular_dielectric = vec3(0.0);
        vec3 l_specular_metal = vec3(0.0);
        vec3 l_dielectric_brdf = vec3(0.0);
        vec3 l_metal_brdf = vec3(0.0);

        l_specular_metal = lightIntensity * NdotL * BRDF_specularGGX(roughness*roughness, NdotL, NdotV, NdotH);
        l_specular_dielectric = l_specular_metal;

        l_metal_brdf = metal_fresnel * l_specular_metal;
        l_dielectric_brdf = mix(l_diffuse, l_specular_dielectric, dielectric_fresnel);
        
        l_light = mix(l_dielectric_brdf, l_metal_brdf, metallic) * shadowFactor;
    }

    // Total color
    vec3 color = mix(f_dielectric_brdf_ibl, f_metal_brdf_ibl, metallic) + l_light;
    
    float u_OcclusionStrength = 1.0;
    color = color * (1.0 + u_OcclusionStrength * (ao - 1.0)); 
    
    color += emissive;

    outColor = vec4(color, 1.0);
}