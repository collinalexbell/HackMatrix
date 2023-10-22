#ifndef __RENDERER_H__
#define __RENDERER_H__
#include "shader.h"
#include "texture.h"
#include "world.h"
#include "camera.h"
#include "app.h"
#include <map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Cube;
class World;
class Renderer {
  unsigned int VBO;
  unsigned int APP_VBO;
  unsigned int INSTANCE;
  unsigned int APP_INSTANCE;
  unsigned int VAO;
  unsigned int APP_VAO;
  unsigned int EBO;

  unsigned int LINE_VBO;
  unsigned int LINE_INSTANCE;
  unsigned int LINE_VAO;

  Shader* shader;
  Shader* appShader;
  std::map<string, Texture*> textures;
  glm::mat4 trans;
  glm::mat4 model;
  glm::mat4 appModel;
  glm::mat4 view;
  glm::mat4 orthographicMatrix;
  void updateTransformMatrices();
  Camera* camera = NULL;
  World* world = NULL;

  unsigned int emacsID;

  float deltaTime = 0.0f;	// Time between current frame and last frame
  float lastFrame = 0.0f; // Time of last frame

  vector<unsigned int> frameBuffers;
  void drawAppDirect(X11App* app);
public:
  Renderer(Camera*, World*);
  ~Renderer();
  Camera* getCamera();
  void render();
  void genGlResources();
  void bindGlResourcesForInit();
  void setupVertexAttributePointers();
  void fillBuffers();
  void addCube(int index, Cube cube);
  void addLine(int index, Line line);
  void addAppCube(int index, glm::vec3 pos);
  void registerApp(X11App* app, int index);
  void deregisterApp(int index);
  void reloadChunk();
  void screenshot();

  glm::mat4 projection;
};

#endif
