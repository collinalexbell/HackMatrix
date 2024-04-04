#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;
uniform mat4 shadowMatrices[6];
out vec4 FragPos; // FragPos from GS (output per emitvertex)
out float Color;

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        
        // Assign a different color to each face
        if (face == 0) Color = 0.0;      
        else if (face == 1) Color = 0.15; 
        else if (face == 2) Color = 0.35;
        else if (face == 3) Color = 0.5;
        else if (face == 4) Color = 0.7;
        else Color = 1;
        
        for (int i = 0; i < 3; ++i) {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
