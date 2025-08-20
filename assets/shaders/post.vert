#version 450

layout (location = 0) out vec2 outTexCoord;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
    // Generate a fullscreen quad using vertex index
    vec2 positions[6] = vec2[](
        vec2(-1.0, -1.0),  // Bottom-left
        vec2( 1.0, -1.0),  // Bottom-right  
        vec2(-1.0,  1.0),  // Top-left
        vec2(-1.0,  1.0),  // Top-left
        vec2( 1.0, -1.0),  // Bottom-right
        vec2( 1.0,  1.0)   // Top-right
    );
    
    // Fix: Remove the Y-flip by mapping screen space correctly to texture space
    vec2 texCoords[6] = vec2[](
        vec2(0.0, 0.0),    // Bottom-left maps to tex(0,0) 
        vec2(1.0, 0.0),    // Bottom-right maps to tex(1,0)
        vec2(0.0, 1.0),    // Top-left maps to tex(0,1)
        vec2(0.0, 1.0),    // Top-left maps to tex(0,1)
        vec2(1.0, 0.0),    // Bottom-right maps to tex(1,0)
        vec2(1.0, 1.0)     // Top-right maps to tex(1,1)
    );
    
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outTexCoord = texCoords[gl_VertexIndex];
}