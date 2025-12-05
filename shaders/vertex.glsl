#version 330 core
layout (location = 0) in vec3 position;
// For lines this is a color, for other geometry it carries texcoords (z unused).
layout (location = 1) in vec3 attr1;
layout (location = 2) in vec3 normal;
layout (location = 7) in vec3 barycentric;

out vec2 TexCoord;
out vec3 lineColor;
out vec4 ModelColor;
out vec3 Normal;
out vec3 FragPos;
out vec3 Barycentric;

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
uniform bool isVoxel;
uniform bool voxelsEnabled;
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
    TexCoord = attr1.xy;
  } else if(isModel) {
    gl_Position = projection * view * model * vec4(position, 1.0);
    FragPos = vec3(model * vec4(position, 1.0));
    TexCoord = attr1.xy;
    Normal = normalMatrix * normal;
    Barycentric = vec3(0.0);
  } else if(isVoxel && voxelsEnabled) {
    vec4 worldPosition = model * vec4(position, 1.0);
    gl_Position = projection * view * worldPosition;
    FragPos = vec3(worldPosition);
    TexCoord = vec2(0.0);
    Normal = mat3(model) * normal;
    Barycentric = barycentric;
  } else if (isLine) {
    gl_Position = projection * view * vec4(position, 1.0);
    FragPos = position;
    TexCoord = vec2(0.0);
    Normal = vec3(0.0);
    Barycentric = vec3(0.0);
    lineColor = attr1;
  } else {
    gl_Position = projection * view * vec4(position, 1.0);
    FragPos = vec3(position);
    TexCoord = vec2(0.0);
    Normal = vec3(0.0, 1.0, 0.0);
    Barycentric = vec3(0.0);
  }
}
