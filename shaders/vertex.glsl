#version 330 core
layout (location = 0) in vec3 aPos;
out float isLeft;
void main()
{
  if(aPos.x < 0.0){
    isLeft=1.0;
  }else{
    isLeft=0.0;
  }
  gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
