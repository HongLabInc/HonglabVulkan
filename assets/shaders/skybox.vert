#version 450

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

layout(location = 0) out vec3 outLocalPos;

out gl_PerVertex {
    vec4 gl_Position;
};

const vec3 pos[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0),
	vec3( 1.0,-1.0, 1.0),
	vec3( 1.0, 1.0, 1.0),
	vec3(-1.0, 1.0, 1.0),

	vec3(-1.0,-1.0,-1.0),
	vec3( 1.0,-1.0,-1.0),
	vec3( 1.0, 1.0,-1.0),
	vec3(-1.0, 1.0,-1.0)
);

const int indices[36] = int[36](
	0, 1, 2, 2, 3, 0,	// front
	1, 5, 6, 6, 2, 1,	// right 
	7, 6, 5, 5, 4, 7,	// back
	4, 0, 3, 3, 7, 4,	// left
	4, 5, 1, 1, 0, 4,	// bottom
	3, 2, 6, 6, 7, 3	// top
);

void main() {
    vec3 localPos = pos[indices[gl_VertexIndex]];
    outLocalPos = localPos;
    
    // Remove translation from view matrix for skybox
    mat4 rotView = mat4(mat3(sceneData.view));
    vec4 clipPos = sceneData.projection * rotView * vec4(localPos, 1.0);
    
    // Ensure skybox is always at far plane
    gl_Position = clipPos.xyww;
}