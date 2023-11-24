#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 lineInstanceColor;
layout (location = 1) in vec2 aTexCoord;
layout (location = 1) in int meshBlockType;
layout (location = 2) in vec3 aOffset;
layout (location = 2) in int meshSelection;
layout (location = 3) in int blockType;
layout (location = 3) in vec2 meshTexCoord;

layout (location = 4) in int selection;

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

uniform vec3 lookedAt;
uniform bool lookedAtValid;

void main()
{
  // model in this case is used per call to glDrawArraysInstanced
  if(isApp) {
    gl_Position = projection * view * appModel * vec4(aPos + aOffset, 1.0);
    BlockType = blockType;
    if (selection > 0) {
      Selection = selection;
    }
    TexCoord = aTexCoord;
  } else if(isLine) {
    gl_Position = projection * view *  vec4(aPos, 1.0);
    lineColor = lineInstanceColor;
  } else if(isMesh) {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    BlockType = meshBlockType;
    if (meshSelection > 0) {
      Selection = meshSelection;
    }
    TexCoord = meshTexCoord;
  } else {
    gl_Position = projection * view * model * vec4(aPos + aOffset, 1.0);
    TexCoord = aTexCoord;
    BlockType = blockType;
    if (selection > 0) {
      Selection = selection;
    }
  }

  IsLookedAt = 0;
  if(lookedAtValid) {
    if(abs(distance(aPos,lookedAt)) <= 0.9) {
      IsLookedAt = 1;
    }
  }
}
