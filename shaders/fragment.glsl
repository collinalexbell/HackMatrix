#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
flat in int BlockType;

uniform sampler2DArray texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform bool isApp;
uniform int totalBlockTypes;

void main()
{
  if(isApp) {
    FragColor = texture(texture3, TexCoord * vec2(1,-1));
  } else {
    FragColor = texture(texture1, vec3(TexCoord.x, TexCoord.y, BlockType));
  }
}
