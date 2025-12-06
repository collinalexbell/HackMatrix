#include "IndexPool.h"
#include "glm/ext/matrix_transform.hpp"
#include "model.h"
#include "components/Light.h"
#include "systems/Intersections.h"
#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include "AppSurface.h"
#include "wayland_app.h"
#include "camera.h"
#include "app.h"
#include "screen.h"
#include "components/Bootable.h"
#include "time_utils.h"
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "tracy/TracyOpenGL.hpp"
#include "tracy/Tracy.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <ctime>
#include <iomanip>
#include <cstring>
#include <cstdio>

static bool
gl_version_at_least(int wantMajor, int wantMinor)
{
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  if (!version) {
    return false;
  }
  // Handles strings like "OpenGL ES 3.2 ...", "OpenGL ES 3.0", "3.3.0"
  int major = 0;
  int minor = 0;
  if (strstr(version, "OpenGL ES")) {
    std::sscanf(version, "%*s %*s %d.%d", &major, &minor);
  } else {
    std::sscanf(version, "%d.%d", &major, &minor);
  }
  return (major > wantMajor) || (major == wantMajor && minor >= wantMinor);
}

static bool
is_gles()
{
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  return version && std::strstr(version, "OpenGL ES");
}
#define DISABLE_CULLING true

float HEIGHT = SCREEN_HEIGHT / SCREEN_WIDTH / 2.0;
float MAX_LIGHTS = 5;

float appVertices[] = {
  -0.5f, -HEIGHT, 0, 0.0f, 0.0f, 0.5f,  -HEIGHT, 0, 1.0f, 0.0f,
  0.5f,  HEIGHT,  0, 1.0f, 1.0f, 0.5f,  HEIGHT,  0, 1.0f, 1.0f,
  -0.5f, HEIGHT,  0, 0.0f, 1.0f, -0.5f, -HEIGHT, 0, 0.0f, 0.0f,
};

float directRenderQuad[] = {
  -1, -1, 0, 0, 0, 1,  -1, 0, 1, 0, 1,  1,  0, 1, 1,
  1,  1,  0, 1, 1, -1, 1,  0, 0, 1, -1, -1, 0, 0, 0
};

void
Renderer::genMeshResources()
{
  glGenVertexArrays(1, &MESH_VERTEX);
  glGenBuffers(1, &MESH_VERTEX_POSITIONS);
  glGenBuffers(1, &MESH_VERTEX_TEX_COORDS);
  glGenBuffers(1, &MESH_VERTEX_BLOCK_TYPES);
  glGenBuffers(1, &MESH_VERTEX_SELECTS);
}

void
Renderer::genDynamicObjectResources()
{
  glGenVertexArrays(1, &DYNAMIC_OBJECT_VERTEX);
  glGenBuffers(1, &DYNAMIC_OBJECT_POSITIONS);
};

void
Renderer::genGlResources()
{
  glGenVertexArrays(1, &APP_VAO);
  glGenBuffers(1, &APP_VBO);

  glGenVertexArrays(1, &DIRECT_RENDER_VAO);
  glGenBuffers(1, &DIRECT_RENDER_VBO);

  glGenVertexArrays(1, &LINE_VAO);
  glGenBuffers(1, &LINE_VBO);
  glGenBuffers(1, &LINE_INSTANCE);

  glGenVertexArrays(1, &VOXEL_SELECTIONS);
  glGenBuffers(1, &VOXEL_SELECTION_POSITIONS);
  glGenBuffers(1, &VOXEL_SELECTION_TEX_COORDS);

  genMeshResources();
  genDynamicObjectResources();
}

void
Renderer::setupMeshVertexAttributePoiners()
{
  glBindVertexArray(MESH_VERTEX);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
  glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void*)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
  glVertexAttribIPointer(3, 1, GL_INT, sizeof(int), (void*)0);
  glEnableVertexAttribArray(3);
}

void
Renderer::setupDynamicObjectVertexAttributePointers()
{
  glBindVertexArray(DYNAMIC_OBJECT_VERTEX);
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

void
Renderer::setupVertexAttributePointers()
{
  setupMeshVertexAttributePoiners();
  setupDynamicObjectVertexAttributePointers();

  glBindVertexArray(APP_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(
    1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // direct render
  glBindVertexArray(DIRECT_RENDER_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, DIRECT_RENDER_VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(
    1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // line
  glBindVertexArray(LINE_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);

  glBindVertexArray(VOXEL_SELECTIONS);
  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
}

int MAX_CUBES = 1000000;

void
Renderer::fillDynamicObjectBuffers()
{
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 30000), (void*)0, GL_DYNAMIC_DRAW);
}

void
Renderer::fillBuffers()
{
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  glBufferData(
    GL_ARRAY_BUFFER, sizeof(appVertices), appVertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, DIRECT_RENDER_VBO);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(directRenderQuad),
               directRenderQuad,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void*)0, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void*)0, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
  glBufferData(GL_ARRAY_BUFFER,
               (sizeof(glm::vec3) * 36 * MAX_CUBES),
               (void*)0,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
  glBufferData(GL_ARRAY_BUFFER,
               (sizeof(glm::vec2) * 36 * MAX_CUBES),
               (void*)0,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(int) * 36 * MAX_CUBES), (void*)0, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(int) * 36 * MAX_CUBES), (void*)0, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 6), (void*)0, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
  glBufferData(
    GL_ARRAY_BUFFER, (sizeof(glm::vec2) * 6), (void*)0, GL_DYNAMIC_DRAW);

  fillDynamicObjectBuffers();
}

void
Renderer::toggleWireframe()
{
  if (isWireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
    isWireframe = false;
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
    isWireframe = true;
  }
}

Renderer::Renderer(shared_ptr<EntityRegistry> registry,
                   Camera* camera,
                   World* world,
                   shared_ptr<blocks::TexturePack> texturePack,
                   bool invertY)
  : texturePack(texturePack)
  , registry(registry)
  , appIndexPool(IndexPool(9))
  , invertY(invertY)
{
  this->camera = camera;
  this->world = world;

  logger = make_shared<spdlog::logger>("Renderer", fileSink);
  logger->set_level(spdlog::level::info);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // glEnable(GL_MULTISAMPLE);
  genGlResources();
  fillBuffers();
  setupVertexAttributePointers();

  std::vector<std::string> images = texturePack->imageNames();
  textures.insert(
    std::pair<string, Texture*>("allBlocks", new Texture(images, GL_TEXTURE0)));
  cameraShader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shadowsEnabled = gl_version_at_least(3, 2);
  if (!shadowsEnabled) {
    logger->warn("Disabling shadows: GL version too low for geometry shader");
  } else {
    depthShader = new Shader("shaders/depthVertex.glsl",
                             "shaders/depthGeometry.glsl",
                             "shaders/depthFragment.glsl");
  }

  shader = cameraShader;

  shader->use(); // may need to move into loop to use changing uniforms

  shader->setInt("allBlocks", 0);
  shader->setInt("totalBlockTypes", images.size());
  shader->setBool("SHADOWS_ENABLED", shadowsEnabled);

  initAppTextures();

  shader->setBool("lookedAtValid", false);
  shader->setBool("isLookedAt", false);
  shader->setBool("isMesh", false);
  shader->setBool("isModel", false);
  shader->setBool("directRender", false);
  shader->setBool("isVoxel", false);
  shader->setBool("voxelsEnabled", voxelsEnabled);

  voxelSpace.add(glm::vec3(0, 4, 4), voxelSize);
  voxelSpace.add(glm::vec3(voxelSize, 4, 4), voxelSize);
  voxelSpace.add(glm::vec3(-voxelSize, 4, 4), voxelSize);
  voxelSpace.add(glm::vec3(0, 4 + voxelSize, 4), voxelSize);
  voxelSpace.add(glm::vec3(0, 4 - voxelSize, 4), voxelSize);
  voxelMesh = voxelSpace.render();

  if (!is_gles()) {
    if (isWireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
    } else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
    }
  }
  glClearColor(163.0 / 255.0, 163.0 / 255.0, 167.0 / 255.0, 1.0f);
  if (!is_gles()) {
    glLineWidth(10.0);
  } else {
    glLineWidth(1.0f);
  }
}

void
Renderer::initAppTextures()
{
  GLint maxUnits = 0;
  // Fragment shader texture units; use these to stay GLES-compatible.
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
  if (maxUnits <= 0) {
    maxUnits = 8;
  }
  // Keep a small pool to avoid exhausting units (GLES2 often has 8).
  int appTextures = std::min(4, maxUnits);
  appIndexPool = IndexPool(appTextures);
  static FILE* logFile = []() {
    FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    return f ? f : stderr;
  }();
  std::fprintf(logFile, "initAppTextures: maxUnits=%d appTextures=%d\n", maxUnits, appTextures);
  for (int index = 0; index < appTextures; index++) {
    int textureN = index;
    int textureUnit = GL_TEXTURE0; // single unit; we'll bind per-app before draw
    string textureName = "app" + to_string(index);
    textures[textureName] = new Texture(textureUnit);
    shader->setInt(textureName, 0); // all samplers map to unit 0
    std::fprintf(logFile,
                 "initAppTextures: index=%d texName=%s texId=%u unit=%d\n",
                 index,
                 textureName.c_str(),
                 textures[textureName]->ID,
                 textureUnit - GL_TEXTURE0);
  }
  std::fflush(logFile);
}

void
Renderer::updateTransformMatrices()
{
  if (camera->viewMatrixUpdated()) {
    unsigned int viewLoc = glGetUniformLocation(shader->ID, "view");
    glUniformMatrix4fv(
      viewLoc, 1, GL_FALSE, glm::value_ptr(camera->getViewMatrix()));
  }
  if (camera->projectionMatrixUpdated()) {
    unsigned int projectionLoc = glGetUniformLocation(shader->ID, "projection");
    glUniformMatrix4fv(projectionLoc,
                       1,
                       GL_FALSE,
                       glm::value_ptr(camera->getProjectionMatrix(true)));
  }
}

void
Renderer::updateChunkMeshBuffers(vector<shared_ptr<ChunkMesh>>& meshes)
{
  verticesInMesh = 0;
  for (auto mesh : meshes) {
    // if(mesh.updated) {
    glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(glm::vec3) * verticesInMesh,
                    sizeof(glm::vec3) * mesh->positions.size(),
                    mesh->positions.data());

    glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(glm::vec2) * verticesInMesh,
                    sizeof(glm::vec2) * mesh->texCoords.size(),
                    mesh->texCoords.data());

    glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(int) * verticesInMesh,
                    sizeof(int) * mesh->blockTypes.size(),
                    mesh->blockTypes.data());

    glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(int) * verticesInMesh,
                    sizeof(int) * mesh->selects.size(),
                    mesh->selects.data());
    //}
    verticesInMesh += mesh->positions.size();
  }
}

void
Renderer::updateDynamicObjects(shared_ptr<DynamicObject> obj)
{
  auto renderable = obj->makeRenderable();
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glBufferSubData(GL_ARRAY_BUFFER,
                  0,
                  sizeof(glm::vec3) * renderable.vertices.size(),
                  renderable.vertices.data());
  verticesInDynamicObjects = renderable.vertices.size();
}

void
Renderer::addLine(int index, Line line)
{
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2) * index,
                  sizeof(glm::vec3),
                  &line.points[0]);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2) * index + (sizeof(glm::vec3)),
                  sizeof(glm::vec3),
                  &line.points[1]);

  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2 * index),
                  (sizeof(glm::vec3)),
                  &line.color);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2 * index + sizeof(glm::vec3)),
                  (sizeof(glm::vec3)),
                  &line.color);
}

void
Renderer::setLines(const std::vector<Line>& lines)
{
  for (size_t i = 0; i < lines.size(); ++i) {
    addLine(static_cast<int>(i), lines[i]);
  }
}

void
Renderer::renderDynamicObjects()
{
  glBindVertexArray(DYNAMIC_OBJECT_VERTEX);
  shader->setBool("isDynamicObject", true);
  glDrawArrays(GL_TRIANGLES, 0, verticesInDynamicObjects);
  shader->setBool("isDynamicObject", false);
}

void
Renderer::drawAppDirect(AppSurface* app, Bootable* bootable)
{
  int index = app->getAppIndex();
  int screenWidth = SCREEN_WIDTH;
  int screenHeight = SCREEN_HEIGHT;
  int appWidth = app->getWidth();
  int appHeight = app->getHeight();
  auto pos = app->getPosition();
  auto fbIt = frameBuffers.find(index);
  if (fbIt == frameBuffers.end()) {
    return;
  }
  if (index >= 0) {
    if (!bootable) {
      GLint prevReadFbo = 0;
      GLint prevDrawFbo = 0;
      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbIt->second);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
      glBlitFramebuffer(
        // src x1, y1 (flip)
        0,
        appHeight,
        // src x2, y2 (flip)
        appWidth,
        0,
        // dest x1,y1,x2,y2
        pos[0],
        pos[1],
        pos[0] + appWidth,
        pos[1] + appHeight,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
    } else {
      glActiveTexture(GL_TEXTURE0);
      auto texIt = textures.find("app" + std::to_string(index));
      if (texIt != textures.end()) {
        glBindTexture(GL_TEXTURE_2D, texIt->second->ID);
      }
      shader->setInt("app0", 0);
      shader->setInt("appNumber", 0);
      glBindVertexArray(DIRECT_RENDER_VAO);
      shader->setBool("directRender", true);
      static int x = -1;
      static int y = -1;
      static glm::mat4 model;
      if (x == -1 || y == -1 || x != bootable->x || y != bootable->y) {
        model = glm::mat4(1.0);

        model = glm::translate(
          model,
          glm::vec3((-0.5 + (float)appWidth / (float)screenWidth / 2 +
                     (float)bootable->x / (float)screenWidth) *
                      2,

                    -((-0.5 + (float)appHeight / (float)screenHeight / 2 +
                       (float)bootable->y / (float)screenHeight) *
                      2),

                    0));

        model = glm::scale(model,
                           glm::vec3((float)appWidth / ((float)screenWidth),

                                     (float)appHeight / ((float)screenHeight),
                                     1));

        x = bootable->x;
        y = bootable->y;
      }
      shader->setBool("appTransparent", bootable->transparent);
      shader->setMatrix4("model", model);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      shader->setBool("directRender", false);
    }
  }
}

void
Renderer::screenshot()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  stbi_flip_vertically_on_write(true);
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  stringstream filenameSS;
  filenameSS << "screenshots/" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S.png");

  string filename = filenameSS.str();

  // Capture the screenshot and save it as a PNG file
  int width = getScreenWidth();  // Width of your rendering area
  int height = getScreenHeight(); // Height of your rendering area
  int channels = 4;  // 4 for RGBA
  unsigned char* data = new unsigned char[width * height * channels];
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
  std::thread saver([filename, width, height, channels, data]() {
    stbi_write_png(
      filename.c_str(), width, height, channels, data, width * channels);
    delete[] data;
  });
  saver.detach();
}

void
Renderer::screenshotFromCurrentFramebuffer(int width, int height)
{
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  stringstream filenameSS;
  filenameSS << "screenshots/" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S.png");

  string filename = filenameSS.str();

  int channels = 4; // 4 for RGBA
  unsigned char* data = new unsigned char[width * height * channels];
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
  std::thread saver([filename, width, height, channels, data]() {
    // Wayland framebuffer is already oriented correctly; avoid flipping.
    stbi_flip_vertically_on_write(false);
    stbi_write_png(
      filename.c_str(), width, height, channels, data, width * channels);
    delete[] data;
  });
  saver.detach();
}

void
Renderer::renderLookedAtFace()
{
  Position lookedAt = world->getLookedAtCube();
  if (lookedAt.valid) {
    ChunkMesh lookedAtFace = world->meshSelectedCube(lookedAt);

    glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    6 * (sizeof(glm::vec3)),
                    lookedAtFace.positions.data());

    glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    6 * (sizeof(glm::vec2)),
                    lookedAtFace.texCoords.data());

    glBindVertexArray(VOXEL_SELECTIONS);
    shader->setBool("isLookedAt", true);
    shader->setInt("lookedAtBlockType", lookedAtFace.blockTypes[0]);
    shader->setBool("isMesh", true);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  shader->setBool("isLookedAt", false);
  shader->setBool("isMesh", false);
}

void
Renderer::updateShaderUniforms()
{
  shader->setFloat("time", nowSeconds());
  shader->setBool("isApp", false);
  shader->setBool("isLine", false);
  shader->setBool("isVoxel", false);
  shader->setBool("voxelsEnabled", voxelsEnabled);
}

void
Renderer::renderChunkMesh()
{
  shader->setBool("isMesh", true);
  glBindVertexArray(MESH_VERTEX);
  // TODO: fix this
  // glEnable(GL_CULL_FACE);
  glDisable(GL_CULL_FACE);
  glDrawArrays(GL_TRIANGLES, 0, verticesInMesh);
  shader->setBool("isMesh", false);
}

void
Renderer::renderApps()
{
  static FILE* logFile = []() {
    FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    return f ? f : stderr;
  }();
  static std::unordered_set<void*> loggedApps;
  TracyGpuZone("render apps");
  auto lookedAtAppEntity = windowManagerSpace->getLookedAtApp();
  shader->setBool("appSelected", false);

  shader->setBool("isApp", true);
  glBindVertexArray(APP_VAO);
  glDisable(GL_CULL_FACE);
  auto bindAppTexture = [&](int index) {
    auto it = textures.find("app" + std::to_string(index));
    if (it != textures.end()) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, it->second->ID);
    }
    shader->setInt("appNumber", 0); // all app samplers map to unit 0
  };

  // this is LEGACY for msedge
  auto positionableNonBootable =
    registry->view<X11App, Positionable>(entt::exclude<Bootable>);
  for (auto [entity, app, positionable] : positionableNonBootable.each()) {
    bindAppTexture(static_cast<int>(app.getAppIndex()));
    shader->setMatrix4("model", positionable.modelMatrix);

    // OPTIMIZATION
    // TODO: cache the recomputeHeightScaler into the app itself and don't recompute it every render
    shader->setMatrix4("bootableScale", app.getHeightScalar());
    shader->setInt("appNumber", 0);
    shader->setBool("appTransparent", false);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  auto positionableApps = registry->view<X11App, Positionable, Bootable>();
  for (auto [entity, app, positionable, bootable] : positionableApps.each()) {
    bindAppTexture(static_cast<int>(app.getAppIndex()));
    shader->setMatrix4("model", positionable.modelMatrix);
    shader->setMatrix4("bootableScale", bootable.getHeightScaler());
    shader->setInt("appNumber", 0);
    if (app.isSelected()) {
      shader->setBool("appSelected", true);
    }
    if (bootable.transparent) {
      shader->setBool("appTransparent", true);
    } else {
      shader->setBool("appTransparent", false);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (loggedApps.insert((void*)&app).second) {
      std::fprintf(logFile,
                   "renderer: first render X11 app idx=%zu size=%dx%d\n",
                   app.getAppIndex(),
                   bootable.getWidth(),
                   bootable.getHeight());
      std::fflush(logFile);
    }
  }

  if (lookedAtAppEntity.has_value()) {
    auto ent = lookedAtAppEntity.value();
    if (registry->all_of<X11App>(ent)) {
      auto bootable = registry->try_get<Bootable>(ent);
      auto& app = registry->get<X11App>(ent);
      if (wm->getCurrentlyFocusedApp().has_value()
          && wm->getCurrentlyFocusedApp().value() == ent
          && (!bootable || !bootable->transparent)) {
        shader->setBool("appFocused", app.isFocused());
        drawAppDirect(&app);
        shader->setBool("appFocused", false);
      }
    }
  }

  auto directRenderBlits =
    registry->view<X11App>(entt::exclude<Positionable, Bootable>);
  for (auto [entity, directApp] : directRenderBlits.each()) {
    bindAppTexture(static_cast<int>(directApp.getAppIndex()));
    drawAppDirect(&directApp);
  }

  auto directRenderNonBlits =
    registry->view<X11App, Bootable>(entt::exclude<Positionable>);
  for (auto [entity, directApp, bootable] : directRenderNonBlits.each()) {
    bindAppTexture(static_cast<int>(directApp.getAppIndex()));
    drawAppDirect(&directApp, &bootable);
  }

  // Wayland apps: render any that were registered by the wlroots backend.
  auto wlPositionable =
    registry->view<WaylandApp::Component, Positionable>(entt::exclude<Bootable>);
  for (auto [entity, comp, positionable] : wlPositionable.each()) {
    auto* app = comp.app.get();
    if (!app) {
      continue;
    }
    bindAppTexture(static_cast<int>(app->getAppIndex()));
    app->appTexture();
    shader->setMatrix4("model", positionable.modelMatrix);
    shader->setMatrix4("bootableScale", app->getHeightScalar());
    shader->setInt("appNumber", 0);
    shader->setBool("appTransparent", false);
    std::fprintf(logFile,
                 "Renderer: Wayland in-world ent=%d appNumber=%zu texId=%d texUnit=%d size=%dx%d\n",
                 (int)entity,
                 app->getAppIndex(),
                 app->getTextureId(),
                 app->getTextureUnit() - GL_TEXTURE0,
                 app->getWidth(),
                 app->getHeight());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    // If focused, also draw directly to screen to ensure visibility.
    if (wm && wm->getCurrentlyFocusedApp().has_value() &&
        wm->getCurrentlyFocusedApp().value() == entity) {
      bindAppTexture(static_cast<int>(app->getAppIndex()));
      shader->setInt("app0", 0);
      shader->setBool("directRender", true);
      shader->setInt("appNumber", 0);
      glm::mat4 model = glm::mat4(1.0f);
      float sx = static_cast<float>(app->getWidth()) /
                 static_cast<float>(SCREEN_WIDTH);
      float sy = static_cast<float>(app->getHeight()) /
                 static_cast<float>(SCREEN_HEIGHT);
      model = glm::scale(model, glm::vec3(sx, sy, 1.0f));
      shader->setMatrix4("model", model);
      std::fprintf(logFile,
                   "Renderer: Wayland direct ent=%d appNumber=%zu texId=%d texUnit=%d screenScale=(%.3f,%.3f)\n",
                   (int)entity,
                   app->getAppIndex(),
                   app->getTextureId(),
                   app->getTextureUnit() - GL_TEXTURE0,
                   sx,
                   sy);
      glBindVertexArray(DIRECT_RENDER_VAO);
      glDisable(GL_DEPTH_TEST);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glEnable(GL_DEPTH_TEST);
      shader->setBool("directRender", false);
      shader->setMatrix4("model", positionable.modelMatrix);
    }
    if (loggedApps.insert((void*)app).second) {
      std::fprintf(logFile,
                   "renderer: first render Wayland app size=%dx%d\n",
                   app->getWidth(),
                   app->getHeight());
      std::fflush(logFile);
    }
  }

  shader->setBool("isApp", false);
}

void
Renderer::renderLines()
{
  shader->setBool("isApp", false);
  shader->setBool("isModel", false);
  shader->setBool("isVoxel", false);
  shader->setBool("isDynamicObject", false);
  shader->setBool("isLine", true);
  // Draw lines on top; depth test off for clear visibility of selection boxes.
  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  if (depthEnabled) {
    glDisable(GL_DEPTH_TEST);
  }
  glLineWidth(2.5f);
  glBindVertexArray(LINE_VAO);
  glDrawArrays(GL_LINES, 0, world->getLines().size() * 2);
  if (depthEnabled) {
    glEnable(GL_DEPTH_TEST);
  }
  glLineWidth(1.0f);
  shader->setBool("isLine", false);
}

void
Renderer::renderVoxels()
{
  if (!voxelsEnabled) {
    return;
  }
  shader->setBool("isVoxel", true);
  shader->setMatrix4("model", glm::mat4(1.0f));
  glDisable(GL_CULL_FACE);
  voxelMesh.draw();
  shader->setBool("isVoxel", false);
}

void
Renderer::addVoxels(const std::vector<glm::vec3>& positions,
                    bool replace,
                    float size,
                    glm::vec3 color)
{
  if (size > 0.0f) {
    voxelSize = size;
  }
  if (replace) {
    voxelSpace.clear();
  }
  if (positions.empty() && replace) {
    voxelMesh = RenderedVoxelSpace();
    voxelsEnabled = false;
    return;
  }
  for (const auto& pos : positions) {
    voxelSpace.add(pos, size, color);
  }
  voxelMesh = voxelSpace.render();
  voxelsEnabled = true;
}

void
Renderer::clearVoxelsInBox(const glm::vec3& minCorner,
                           const glm::vec3& maxCorner)
{
  auto removed = voxelSpace.remove(minCorner, maxCorner);
  if (removed == 0) {
    return;
  }
  voxelMesh = voxelSpace.render();
  voxelsEnabled = !voxelSpace.empty();
}

bool
Renderer::voxelExistsAt(const glm::vec3& worldPosition, float size) const
{
  float s = size > 0.0f ? size : voxelSize;
  glm::vec3 pos = worldPosition;
  // Basic epsilon to tolerate float drift when comparing grid-aligned voxels.
  const float EPS = 0.0001f;
  glm::vec3 expected = glm::round(pos / s) * s;
  if (glm::length(pos - expected) > EPS) {
    pos = expected;
  }

  // VoxelSpace currently only exposes add/render; mirror the simple storage
  // here by iterating the backing container.
  return voxelSpace.has(pos, s);
}

void
Renderer::lightUniforms(RenderPerspective perspective,
                        std::optional<entt::entity> fromLight)
{
  auto lightView = registry->view<Light, Positionable>();
  int lightIndex = 0;
  int lastTextureUnit;
  for (auto [entity, light, positionable] : lightView.each()) {
    shader->setVec3("lightPos[" + std::to_string(lightIndex) + "]",
                    positionable.pos);
    shader->setVec3("lightColor[" + std::to_string(lightIndex) + "]",
                    light.color);
    shader->setFloat("far_plane[" + std::to_string(lightIndex) + "]",
                     light.farPlane);
    if (perspective == LIGHT && fromLight == entity) {
      shader->setInt("fromLightIndex", lightIndex);
      cout << "lightIndex: " << lightIndex << endl;
      for (unsigned int i = 0; i < 6; ++i) {
        shader->setMatrix4("shadowMatrices[" + std::to_string(i) + "]",
                           light.shadowTransforms[i]);
      }
    }
    if (perspective == CAMERA) {
      shader->setInt("depthCubeMap" + std::to_string(lightIndex),
                     light.textureUnit);
      // THIS IS A HACK
      lastTextureUnit = light.textureUnit;
    }

    // THIS IS A HACK. If I exceed 5 lights, use texture array (proper)
    for (int i = lightIndex; i < MAX_LIGHTS; i++) {
      shader->setInt("depthCubeMap" + std::to_string(i), light.textureUnit);
    }
    lightIndex++;
  }
  shader->setInt("numLights", lightIndex);
}

void
Renderer::renderModels(RenderPerspective perspective)
{
  TracyGpuZone("render models");
  if(perspective != LIGHT) {
    glEnable(GL_CULL_FACE);
  }
  auto frustum = camera->createFrustum();
  shader->setBool("isModel", true);
  shader->setVec3("viewPos", camera->position);
  shader->setBool("isLight", false);

  auto modelView = registry->view<Positionable, Model>();

  bool hasLight = false;

  set<entt::entity> lightEntities;
  auto lightView = registry->view<Light, Positionable>();
  for (auto [entity, light, positionable] : lightView.each()) {
    lightEntities.insert(entity);
  }

  static int lastCount = 0;
  int count = 0;
  for (auto [entity, p, m] : modelView.each()) {
    bool shouldDraw = systems::isOnFrustum(registry, entity, frustum);
    if(DISABLE_CULLING) {
      shouldDraw = true;
    }
    if (!shouldDraw) {
      continue;
    }
    if (!lightEntities.empty() && lightEntities.contains(entity)) {
      shader->setBool("isLight", true);
      if (perspective == LIGHT) {
        shouldDraw = false;
      }
    }
    auto normalMatrix = p.normalMatrix;
    auto modelMatrix = p.modelMatrix;
    shader->setMatrix3("normalMatrix", normalMatrix);
    shader->setMatrix4("model", modelMatrix);

    if (shouldDraw) {
      count++;
      m.Draw(*shader);
    }
    shader->setBool("isLight", false);
  }
  if (count != lastCount) {
    stringstream countSS;
    countSS << "model render count: " << count;
    logger->debug(countSS.str());
    lastCount = count;
  }
  shader->setBool("isModel", false);
}

void
Renderer::render(RenderPerspective perspective,
                 std::optional<entt::entity> fromLight)
{
  ZoneScoped;
  TracyGpuZone("render");
  static FILE* logFile = []() {
    FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    return f ? f : stderr;
  }();
  auto allWl = registry->view<WaylandApp::Component>();
  std::fprintf(logFile, "Renderer: frame WaylandApp count=%zu\n", allWl.size());
  std::fflush(logFile);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glFrontFace(invertY ? GL_CW : GL_CCW);
  if (perspective == CAMERA) {
    shader = cameraShader;
    shader->use();
    // must use prior to updating uniforms

    camera->tick();
    updateTransformMatrices();
  } else {
    if (!shadowsEnabled || depthShader == nullptr) {
      return;
    }
    shader = depthShader;
    shader->use();
  }
  updateShaderUniforms();
  lightUniforms(perspective, fromLight);
  renderModels(perspective);
  if (perspective == CAMERA) {
    renderVoxels();
    renderApps();
  }
  //renderChunkMesh();
}

Camera*
Renderer::getCamera()
{
  return camera;
}

void
Renderer::registerApp(AppSurface* app)
{
  // We need to keep track of which textureN has been used.
  // because deletions means this won't work
  // indices will change.
  auto index = appIndexPool.acquireIndex();
  static GLint maxTextureUnits = -1;
  if (maxTextureUnits < 0) {
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    if (maxTextureUnits <= 0) {
      maxTextureUnits = 8;
    }
  }
  if (index >= maxTextureUnits) {
    std::fprintf(stderr,
                 "Renderer: app index %u exceeds max texture units %d; skipping registration\n",
                 static_cast<unsigned>(index),
                 maxTextureUnits);
    appIndexPool.relinquishIndex(index);
    return;
  }
  FILE* logFile = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
  try {
    int textureId = textures["app" + to_string(index)]->ID;
    int textureUnit = GL_TEXTURE0; // always bind to unit 0; shader samplers set to 0
    app->attachTexture(textureUnit, textureId, index);
    app->appTexture();
    if (logFile) {
      std::fprintf(logFile,
                   "registerApp: index=%u textureId=%d unit=%d\n",
                   static_cast<unsigned>(index),
                   textureId,
                   textureUnit - GL_TEXTURE0);
    }
  } catch (...) {
    if (logFile) {
      std::fprintf(logFile, "registerApp: exception, releasing index\n");
      std::fclose(logFile);
    }
    appIndexPool.relinquishIndex(index);
    throw;
  }
  if (logFile) {
    std::fflush(logFile);
    std::fclose(logFile);
  }

  unsigned int framebufferId;
  glGenFramebuffers(1, &framebufferId);
  frameBuffers[index] = framebufferId;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferId);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         textures["app" + to_string(index)]->ID,
                         0);
  FILE* logFile2 = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
  if (logFile2) {
    std::fprintf(logFile2,
                 "registerApp: framebuffer %u attached to texId=%u index=%u\n",
                 framebufferId,
                 textures["app" + to_string(index)]->ID,
                 static_cast<unsigned>(index));
    std::fflush(logFile2);
    std::fclose(logFile2);
  }
}

void
Renderer::deregisterApp(int index)
{
  appIndexPool.relinquishIndex(index);
  glDeleteFramebuffers(1, &frameBuffers[index]);
  frameBuffers.erase(index);
}

void
Renderer::attachSharedAppTexture(AppSurface* app)
{
  if (!app) {
    return;
  }
  // Use index 0 slot, unit 0; avoids consuming extra texture units.
  auto it = textures.find("app0");
  if (it == textures.end()) {
    return;
  }
  int textureUnit = GL_TEXTURE0;
  int textureId = it->second->ID;
  app->attachTexture(textureUnit, textureId, 0);
  app->appTexture();
  FILE* logFile = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
  if (logFile) {
    std::fprintf(logFile,
                 "attachSharedAppTexture: texId=%d unit=%d\n",
                 textureId,
                 textureUnit - GL_TEXTURE0);
    std::fflush(logFile);
    std::fclose(logFile);
  }
}

void
Renderer::toggleMeshing()
{
  logger->debug("toggleMeshing");
  logger->flush();
}

void
Renderer::wireWindowManager(shared_ptr<WindowManager::WindowManager> wm,
                            shared_ptr<WindowManager::Space> windowManagerSpace)
{
  this->wm = wm;
  this->windowManagerSpace = windowManagerSpace;
}

Renderer::~Renderer()
{
  delete shader;
  for (auto& t : textures) {
    delete t.second;
  }
}
