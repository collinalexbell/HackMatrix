#ifndef __RENDERER_H__
#include "shader.h"
#include "texture.h"
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Renderer {
  unsigned int VBO;
  unsigned int VAO;
  unsigned int EBO;
  Shader* shader;
  std::map<string, Texture*> textures;
  glm::mat4 trans;
  float angle;
  void computeTransform();

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
