#version 330 core
in float isLeft;
uniform float red;
out vec4 FragColor;
void main()
{
  FragColor = vec4(red, isLeft, 0.2f, 1.0f);
}
