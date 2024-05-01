#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normal;

out vec2 TexCoord;
out vec3 lineColor;
out vec4 ModelColor;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 meshModel;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 bootableScale;
uniform mat3 normalMatrix;
uniform bool isApp;
uniform bool isLine;
uniform bool isMesh;
uniform bool isLookedAt;
uniform bool isDynamicObject;
uniform bool isModel;
uniform bool directRender;
uniform int lookedAtBlockType;
uniform int appNumber;

void main()
{
  // model in this case is used per call to glDrawArraysInstanced
  if(isApp) {
    if(directRender) {
      gl_Position = model * vec4(position, 1.0);
    } else {
      gl_Position = projection * view * model * bootableScale * vec4(position, 1.0);
    }
    TexCoord = texCoord;
  } else if(isModel) {
    gl_Position = projection * view * model * vec4(position, 1.0);
    FragPos = vec3(model * vec4(position, 1.0));
    TexCoord = texCoord;
    Normal = normalMatrix * normal;
  }
}
