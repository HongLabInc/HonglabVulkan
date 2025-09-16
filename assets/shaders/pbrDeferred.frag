#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;
layout(location = 5) in vec3 fragCameraPos;
layout(location = 6) in vec4 fragPosLightSpace;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint materialIndex;
    float coeffs[15];    
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
} options;

// Material structure matching MaterialUBO in C++
struct MaterialUBO {
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
};

// Bindless material storage buffer
layout(set = 1, binding = 0) restrict readonly buffer MaterialBuffer {
    MaterialUBO materials[];
} materialBuffer;

// Material textures
layout(set = 1, binding = 1) uniform sampler2D materialTextures[512]; 
// 512 is TextureManager::kMaxTextures_

// G-Buffer outputs for deferred rendering
layout(location = 0) out vec4 gAlbedo;      // RGB: albedo/baseColor, A: metallic
layout(location = 1) out vec4 gNormal;      // RGB: world-space normal (encoded), A: roughness
layout(location = 2) out vec4 gPosition;    // RGB: world-space position, A: depth
layout(location = 3) out vec4 gMaterial;    // R: AO, G: emissive intensity, B: material ID, A: unused

// Normal encoding function for G-buffer
vec3 encodeNormal(vec3 normal) {
    return normal * 0.5 + 0.5;
}

void main() {
    float specularWeight = pushConstants.coeffs[0];
    float diffuseWeight = pushConstants.coeffs[1];
    float emissiveWeight = pushConstants.coeffs[2];
    float shadowOffset = pushConstants.coeffs[3];

    // Access material using push constant index
    MaterialUBO material = materialBuffer.materials[pushConstants.materialIndex];

    // Sample material properties using bindless access
    vec4 baseColorRGBA = (options.textureOn != 0 && material.baseColorTextureIndex >= 0) ? 
                        texture(materialTextures[nonuniformEXT(material.baseColorTextureIndex)], fragTexCoord) : 
                        vec4(1.0);

    if(options.discardOn != 0 && material.opacityTextureIndex >= 0)
    {
        float opacity = texture(materialTextures[nonuniformEXT(material.opacityTextureIndex)], fragTexCoord).r;
        if(opacity < 0.08)
            discard;
    }

    vec3 baseColor = material.baseColorFactor.rgb * baseColorRGBA.rgb;
    float metallic = material.metallicFactor * pushConstants.coeffs[4];
    float roughness = material.roughnessFactor * pushConstants.coeffs[5];

    if(options.textureOn != 0 && material.metallicRoughnessTextureIndex >= 0){
        vec3 metallicRoughness = texture(materialTextures[nonuniformEXT(material.metallicRoughnessTextureIndex)], fragTexCoord).rgb;
        metallic *= metallicRoughness.b; // Blue channel
        roughness *= metallicRoughness.g; // Green channel
    }

    float ao = 1.0;
    if(options.textureOn != 0 && material.occlusionTextureIndex >= 0){
        ao = texture(materialTextures[nonuniformEXT(material.occlusionTextureIndex)], fragTexCoord).r;
    }

    vec3 emissive = material.emissiveFactor.xyz * emissiveWeight;
    if(options.textureOn != 0 && material.emissiveTextureIndex >= 0){
        emissive *= texture(materialTextures[nonuniformEXT(material.emissiveTextureIndex)], fragTexCoord).rgb;
    }

    // Calculate world-space normal with normal mapping
    vec3 N = normalize(fragNormal);
    if(options.textureOn != 0 && material.normalTextureIndex >= 0) {
        vec3 T = normalize(fragTangent);
        vec3 B = normalize(fragBitangent);
        mat3 TBN = mat3(T, B, N);
        
        vec3 tangentNormal = texture(materialTextures[nonuniformEXT(material.normalTextureIndex)], fragTexCoord).xyz * 2.0 - 1.0;
        if (length(tangentNormal) > 0.5)
            N = normalize(TBN * tangentNormal);
    }
    
    // Clamp material properties
    roughness = clamp(roughness, 0.0, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // Calculate emissive intensity for storage
    float emissiveIntensity = length(emissive);

    // Output to G-Buffer (this is what deferred rendering needs)
    gAlbedo = vec4(baseColor, metallic);
    gNormal = vec4(encodeNormal(N), roughness);
    gPosition = vec4(fragPos, gl_FragCoord.z);
    gMaterial = vec4(ao, emissiveIntensity, float(pushConstants.materialIndex) / 255.0, 1.0);
}
