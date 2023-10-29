#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aOffset;
layout (location = 3) in int blockType;

layout (location = 4) in vec3 lineInstanceColor;
layout (location = 4) in int isSelectedVert;

out vec2 TexCoord;
out vec3 lineColor;
flat out int BlockType;
flat out int isSelected;

uniform mat4 model;
uniform mat4 appModel;
uniform mat4 view;
uniform mat4 projection;
uniform bool isApp;
uniform bool isLine;

uniform vec3 selected;
uniform bool selectedValid;

void main()
{
  // model in this case is used per call to glDrawArraysInstanced
  if(isApp) {
    gl_Position = projection * view * appModel * vec4(aPos + aOffset, 1.0);
  } else if(isLine) {
    gl_Position = projection * view *  vec4(aPos, 1.0);
    lineColor = lineInstanceColor;
  } else {
    gl_Position = projection * view * model * vec4(aPos + aOffset, 1.0);
  }

  isSelected = 0;
  if(selectedValid) {
    if(abs(distance(aOffset,selected)) < 0.01) {
      isSelected = 1;
    }
  }
  if (isSelectedVert == 1) {
    isSelected = 2;
  }

  TexCoord = aTexCoord;
  BlockType = blockType;
}
