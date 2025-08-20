#version 450

// Vertex input attributes matching the updated Vertex class layout
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;     // normal (not used for shadow mapping)
layout(location = 2) in vec2 inTexCoord;   // texCoord (not used for shadow mapping)
layout(location = 3) in vec3 inTangent;    // tangent (not used for shadow mapping)
layout(location = 4) in vec3 inBitangent;  // bitangent (not used for shadow mapping)
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

// Push constants for light space matrix
layout(push_constant) uniform ShadowPushConstants {
    mat4 model;
} pushConstants;

void main() {
    vec3 position = inPosition;
    
    // Check animation flag from animationData.x
    bool hasAnimationEnabled = (boneData.animationData.x > 0.5);
    
    // Apply skeletal animation if enabled and vertex has valid bone data
    if (hasAnimationEnabled && (inBoneIndices.x >= 0 || inBoneIndices.y >= 0 || 
                                inBoneIndices.z >= 0 || inBoneIndices.w >= 0)) {
        
        // Calculate animated position
        vec4 animatedPosition = vec4(0.0);
        
        // Apply bone transformations for up to 4 bones per vertex
        for (int i = 0; i < 4; i++) {
            int boneIndex = inBoneIndices[i];
            float weight = inBoneWeights[i];
            
            if (boneIndex >= 0 && boneIndex < 256 && weight > 0.0) {
                mat4 boneMatrix = boneData.boneMatrices[boneIndex];
                
                // Transform position
                animatedPosition += weight * (boneMatrix * vec4(inPosition, 1.0));
            }
        }
        
        // Use animated position if any bone transformations were applied
        if (animatedPosition.w > 0.0) {
            position = animatedPosition.xyz;
        }
    }
    
    // Transform vertex position from object space to world space
    vec4 worldPos = pushConstants.model * vec4(position, 1.0);
    
    // Transform world position to light space (light's view-projection)
    gl_Position = sceneData.lightSpaceMatrix * worldPos;
}