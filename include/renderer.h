#ifndef __RENDERER_H__
#define __RENDERER_H__
#include "chunk.h"
#include "cube.h"
#include "shader.h"
#include "texture.h"
#include "world.h"
#include "camera.h"
#include "app.h"
#include "logger.h"
#include <map>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Cube;
class World;
class Renderer {
  unsigned int APP_VBO;
  unsigned int APP_INSTANCE;
  unsigned int APP_VAO;

  unsigned int LINE_VBO;
  unsigned int LINE_INSTANCE;
  unsigned int LINE_VAO;

  unsigned int MESH_VERTEX;
  unsigned int MESH_VERTEX_POSITIONS;
  unsigned int MESH_VERTEX_TEX_COORDS;
  unsigned int MESH_VERTEX_BLOCK_TYPES;
  unsigned int MESH_VERTEX_SELECTS;

  unsigned int VOXEL_SELECTIONS;
  unsigned int VOXEL_SELECTION_POSITIONS;
  unsigned int VOXEL_SELECTION_TEX_COORDS;

  bool isWireframe = false;

  Shader* shader;
  Shader* appShader;
  std::map<string, Texture*> textures;
  void initAppTextures();
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
  void updateShaderUniforms();
  void renderChunkMesh();
  void renderApps();
  void renderLines();
  void renderLookedAtFace();
  std::shared_ptr<spdlog::logger> logger;
  void genMeshResources();
  void setupMeshVertexAttributePoiners();

  void genGlResources();
  void fillBuffers();
  void setupVertexAttributePointers();

  int verticesInMesh = 0;

public:
  Renderer(Camera*, World*);
  ~Renderer();
  Camera* getCamera();
  void render();
  void updateChunkMeshBuffers(vector<ChunkMesh> meshes);
  void addLine(int index, Line line);
  void addAppCube(int index, glm::vec3 pos);
  void registerApp(X11App* app, int index);
  void deregisterApp(int index);
  void reloadChunk();
  void screenshot();
  void toggleMeshing();
  void toggleWireframe();

  glm::mat4 projection;
};

#endif
