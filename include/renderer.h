#ifndef __RENDERER_H__
#include "shader.h"
#include "texture.h"
#include "world.h"
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <glad/glad.h>
#include <GLFW/glfw3.h>

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
  glm::vec3 cameraPos;
  glm::vec3 cameraFront;
  glm::vec3 cameraUp;
  glm::vec3 cameraTarget;
  glm::vec3 cameraDirection;
  glm::vec3 direction;
  float yaw;
  float pitch;
  float angle;
  void computeTransform();
  void updateTransformMatrices();
  void moveCamera();
  bool firstMouse;
  float lastX;
  float lastY;

 public:
  Renderer();
  ~Renderer();
  void render();
  void render(World* world, float camera[3]);
  void genGlResources();
  void bindGlResourcesForInit();
  void setupVertexAttributePointers();
  void fillBuffers();
  void handleKeys(bool up, bool down, bool left, bool right);
  void mouseCallback (GLFWwindow* window, double xpos, double ypos);
};

#endif
