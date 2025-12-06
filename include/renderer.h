#ifndef __RENDERER_H__
#define __RENDERER_H__
#include "IndexPool.h"
#include "WindowManager/WindowManager.h"
#include "blocks.h"
#include "dynamicObject.h"
#include "entity.h"
#include "components/Bootable.h"
#include "shader.h"
#include "texture.h"
#include "Voxel/VoxelSpace.h"
#include "world.h"
#include "camera.h"
#include "app.h"
#include "WindowManager/Space.h"
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

enum RenderPerspective
{
  CAMERA,
  LIGHT
};

class Cube;
class World;
class Renderer
{
  shared_ptr<blocks::TexturePack> texturePack;
  unsigned int APP_VBO;
  unsigned int APP_VAO;

  unsigned int DIRECT_RENDER_VBO;
  unsigned int DIRECT_RENDER_VAO;

  unsigned int LINE_VBO;
  unsigned int LINE_INSTANCE;
  unsigned int LINE_VAO;

  unsigned int MESH_VERTEX;
  unsigned int MESH_VERTEX_POSITIONS;
  unsigned int MESH_VERTEX_TEX_COORDS;
  unsigned int MESH_VERTEX_BLOCK_TYPES;
  unsigned int MESH_VERTEX_SELECTS;

  unsigned int DYNAMIC_OBJECT_VERTEX;
  unsigned int DYNAMIC_OBJECT_POSITIONS;

  unsigned int VOXEL_SELECTIONS;
  unsigned int VOXEL_SELECTION_POSITIONS;
  unsigned int VOXEL_SELECTION_TEX_COORDS;

  bool isWireframe = false;

  Shader* shader;
  Shader* cameraShader;
  Shader* appShader;
  Shader* depthShader;
  bool invertY = false;
  std::map<string, Texture*> textures;
  void initAppTextures();
  glm::mat4 trans;
  glm::mat4 view;
  glm::mat4 orthographicMatrix;
  void updateTransformMatrices();
  Camera* camera = NULL;
  World* world = NULL;
  shared_ptr<WindowManager::Space> windowManagerSpace;
  shared_ptr<WindowManager::WindowManager> wm;

  unsigned int emacsID;

  float deltaTime = 0.0f; // Time between current frame and last frame
  float lastFrame = 0.0f; // Time of last frame

  unordered_map<int, unsigned int> frameBuffers;
  void drawAppDirect(X11App* app, Bootable* bootable = NULL);
  void updateShaderUniforms();
  void renderChunkMesh();
  void renderApps();
  void renderLines();
  void renderLookedAtFace();
  void renderDynamicObjects();
  void renderModels(RenderPerspective);
  void renderVoxels();
  std::shared_ptr<spdlog::logger> logger;
  void genMeshResources();

  void genDynamicObjectResources();
  void fillDynamicObjectBuffers();
  void setupDynamicObjectVertexAttributePointers();

  void setupMeshVertexAttributePoiners();
  void genGlResources();
  void fillBuffers();
  void setupVertexAttributePointers();
  void lightUniforms(RenderPerspective perspective,
                     std::optional<entt::entity> fromLight);

  int verticesInMesh = 0;
  int verticesInDynamicObjects = 0;

  IndexPool appIndexPool;
  bool voxelsEnabled = true;
  VoxelSpace voxelSpace;
  RenderedVoxelSpace voxelMesh;
  float voxelSize = 2.0f;
  bool shadowsEnabled = true;

public:
  Renderer(shared_ptr<EntityRegistry> registry,
           Camera*,
           World*,
           shared_ptr<blocks::TexturePack>,
           bool invertY = false);
  ~Renderer();
  shared_ptr<EntityRegistry> registry;
  Camera* getCamera();
  void render(RenderPerspective = CAMERA,
              std::optional<entt::entity> = std::nullopt);
  void updateDynamicObjects(shared_ptr<DynamicObject> obj);
  void updateChunkMeshBuffers(vector<shared_ptr<ChunkMesh>>& meshes);
  void addLine(int index, Line line);
  void registerApp(X11App* app);
  void deregisterApp(int index);
  void reloadChunk();
  void screenshot();
  void toggleMeshing();
  void toggleWireframe();
  void wireWindowManager(shared_ptr<WindowManager::WindowManager>, shared_ptr<WindowManager::Space>);
  void addVoxels(const std::vector<glm::vec3>& positions,
                 bool replace = false,
                 float size = 1.0f,
                 glm::vec3 color = glm::vec3(1.0f));
  void clearVoxelsInBox(const glm::vec3& minCorner,
                        const glm::vec3& maxCorner);
  void setLines(const std::vector<Line>& lines);
  float getVoxelSize() const { return voxelSize; }
  bool voxelExistsAt(const glm::vec3& worldPosition, float size) const;

  glm::mat4 projection;
};

#endif
