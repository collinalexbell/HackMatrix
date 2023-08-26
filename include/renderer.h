#ifndef __RENDERER_H__

class Renderer {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int vertexShader;
  unsigned int fragmentShader;
  unsigned int shaderProgram;
  void initFragmentShader();
  void initVertexShader();
  void buildShaderProgram();
 public:
  Renderer();
  void render();
};

#endif
