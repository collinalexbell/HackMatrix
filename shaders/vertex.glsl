#version 330 core
layout (location = 0) in vec3 position;
layout (location = 0) in vec3 vertexPositionInModel;
layout (location = 1) in vec3 lineInstanceColor;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 modelOffset;
layout (location = 2) in int selection;
layout (location = 3) in int blockType;

out vec2 TexCoord;
out vec3 lineColor;
flat out int BlockType;
flat out int IsLookedAt;
flat out int Selection;

uniform mat4 model;
uniform mat4 appModel;
uniform mat4 view;
uniform mat4 projection;
uniform bool isApp;
uniform bool isLine;
uniform bool isMesh;
uniform bool isLookedAt;
uniform bool isDynamicObject;
uniform int lookedAtBlockType;

void main()
{
  // model in this case is used per call to glDrawArraysInstanced
  if(isApp) {
    gl_Position = projection * view * appModel * vec4(vertexPositionInModel + modelOffset, 1.0);
    BlockType = blockType;
    TexCoord = texCoord;
  } else if(isLine) {
    gl_Position = projection * view *  vec4(position, 1.0);
    lineColor = lineInstanceColor;
  } else if(isDynamicObject) {
    gl_Position = projection * view * vec4(position, 1.0);
  } else if(isMesh) {
    gl_Position = projection * view * model * vec4(position, 1.0);
    BlockType = blockType;
    if (selection > 0) {
      Selection = selection;
    }
    TexCoord = texCoord;
  }

  IsLookedAt = 0;
  if(isLookedAt) {
    IsLookedAt = 1;
    BlockType = lookedAtBlockType;
  }
}
