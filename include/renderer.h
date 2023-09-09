#ifndef __RENDERER_H__
#include "shader.h"
#include "texture.h"
#include "world.h"
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
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 orthographicMatrix;
  float angle;
  void computeTransform();
  void updateTransformMatrices();

 public:
  Renderer();
  ~Renderer();
  void render();
  void render(World* world, float camera[3]);
  void genGlResources();
  void bindGlResourcesForInit();
  void setupVertexAttributePointers();
  void fillBuffers();
};

#endif
