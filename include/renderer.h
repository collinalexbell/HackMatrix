#ifndef __RENDERER_H__
#include "shader.h"

class Renderer {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int EBO;
  Shader* shader;
 public:
  Renderer();
  ~Renderer();
  void render();
};

#endif
