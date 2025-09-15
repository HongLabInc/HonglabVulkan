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

layout(location = 0) out vec3 outLocalPos;

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

layout(push_constant) uniform ShadowPushConstants {
    mat4 model;
    mat4 dummy;
} pushConstants; // TODO: dummy, remove

void main() {
    vec3 localPos = pos[indices[gl_VertexIndex]];
    outLocalPos = localPos;
    
    // Remove translation from view matrix for skybox
    mat4 rotView = mat4(mat3(sceneData.view));
    vec4 clipPos = sceneData.projection * rotView * vec4(localPos, 1.0);
    
    // Ensure skybox is always at far plane
    gl_Position = clipPos.xyww;
}