#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aOffset;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
  // model in this case is used per call to glDrawArraysInstanced
  gl_Position = projection * view * model * vec4(aPos + aOffset, 1.0);
  TexCoord = aTexCoord;
}
