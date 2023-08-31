#ifndef __RENDERER_H__
#include "shader.h"
#include "texture.h"

class Renderer {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int EBO;
  Shader* shader;
  Texture* containerTexture;
  Texture* faceTexture;
 public:
  Renderer();
  ~Renderer();
  void render();
  void genGlResources();
  void bindGlResourcesForInit();
  void setupVertexAttributePointers();
  void fillBuffers();
};

#endif
