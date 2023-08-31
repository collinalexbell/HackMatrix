#ifndef __RENDERER_H__
#include "shader.h"
#include "texture.h"
#include <map>

class Renderer {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int EBO;
  Shader* shader;
  std::map<string, Texture*> textures;

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
