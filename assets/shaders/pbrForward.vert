#version 450

// Vertex input attributes matching the updated Vertex class layout
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inBoneWeights;   // NEW: Bone weights for skeletal animation
layout(location = 6) in ivec4 inBoneIndices;  // NEW: Bone indices for skeletal animation

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
    bool textureOn;
    bool shadowOn;
    bool discardOn;
    bool animationOn;
    float ssaoRadius;
    float ssaoBias;
    int ssaoSampleCount;
    float ssaoPower;
} options;

layout(set = 0, binding = 2) uniform BoneDataUBO {
    mat4 boneMatrices[256];  // Support up to 256 bones (16,384 bytes)
    vec4 animationData;      // x = hasAnimation (0.0/1.0), y,z,w = future use
} boneData;

// Push constants for various coefficients
layout(push_constant) uniform PushConstants {
    mat4 model;
    float coeffs[16];
} pushConstants;

// Output to fragment shader
layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;
layout(location = 5) out vec3 fragCameraPos;
layout(location = 6) out vec4 fragPosLightSpace;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;
    vec3 tangent = inTangent;
    vec3 bitangent = inBitangent;
    
    bool animationApplied = false;

    // UPDATED: Check animation flag from animationData.x
    bool hasAnimationEnabled = (boneData.animationData.x > 0.5);
    
    // Apply skeletal animation if enabled and vertex has valid bone data
    if (hasAnimationEnabled && (inBoneIndices.x >= 0 || inBoneIndices.y >= 0 || 
                                inBoneIndices.z >= 0 || inBoneIndices.w >= 0)) {
        
        // Calculate animated position
        vec4 animatedPosition = vec4(0.0);
        vec3 animatedNormal = vec3(0.0);
        vec3 animatedTangent = vec3(0.0);
        vec3 animatedBitangent = vec3(0.0);
        
        // Apply bone transformations for up to 4 bones per vertex
        for (int i = 0; i < 4; i++) {
            int boneIndex = inBoneIndices[i];
            float weight = inBoneWeights[i];
            
            if (boneIndex >= 0 && boneIndex < 256 && weight > 0.0) {
                
                mat4 boneMatrix = boneData.boneMatrices[boneIndex];
                
                // Transform position
                animatedPosition += weight * (boneMatrix * vec4(inPosition, 1.0));
                
                // Transform normal (using upper 3x3 matrix)
                mat3 boneNormalMatrix = mat3(boneMatrix);
                animatedNormal += weight * (boneNormalMatrix * inNormal);
                animatedTangent += weight * (boneNormalMatrix * inTangent);
                animatedBitangent += weight * (boneNormalMatrix * inBitangent);

                animationApplied = true; // DEBUG: Track if any bone transformation was applied
            }
        }
        
        // Use animated attributes if any bone transformations were applied
        if (animatedPosition.w > 0.0) {
            position = animatedPosition.xyz;
            normal = normalize(animatedNormal);
            tangent = normalize(animatedTangent);
            bitangent = normalize(animatedBitangent);
        }
    }
    
    // DEBUG: Apply a small offset if animation was applied (for visual debugging)
    if (animationApplied && hasAnimationEnabled) {
        position.y += sin(gl_VertexIndex * 0.1) * 0.01; // Small visual indicator
    }

    // Transform vertex position to world space
    vec4 worldPos = pushConstants.model * vec4(position, 1.0);
    fragPos = worldPos.xyz;
    
    const mat4 scaleBias = mat4(
        0.5, 0.0, 0.0, 0.0, 
        0.0, 0.5, 0.0, 0.0, 
        0.0, 0.0, 1.0, 0.0, 
        0.5, 0.5, 0.0, 1.0
    );

    // Transform to light space for shadow mapping
    fragPosLightSpace = scaleBias * sceneData.lightSpaceMatrix * worldPos;
    
    // Transform normal, tangent, and bitangent to world space
    mat3 normalMatrix = transpose(inverse(mat3(pushConstants.model)));
    fragNormal = normalMatrix * normal;
    fragTangent = normalMatrix * tangent;
    fragBitangent = normalMatrix * bitangent;
    
    // Pass through texture coordinates and camera position
    fragTexCoord = inTexCoord;
    fragCameraPos = sceneData.cameraPos;
    
    // Transform vertex to clip space
    gl_Position = sceneData.projection * sceneData.view * worldPos;
}