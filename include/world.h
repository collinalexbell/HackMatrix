#ifndef __WORLD_H__

class World {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int vertexShader;
  unsigned int fragmentShader;
  unsigned int shaderProgram;
 public:
  World();
  void render();
};

#endif
