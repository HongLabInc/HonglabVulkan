#version 450

// Fragment shader for shadow map generation
// This shader is minimal since we only need depth values for shadow mapping
// The depth values are automatically written to the depth buffer

layout(push_constant) uniform ShadowPushConstants {
    mat4 model;
    mat4 dummy;
} pushConstants; // TODO remove

void main() 
{
}