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
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "tracy/TracyOpenGL.hpp"
#include "tracy/Tracy.hpp"

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

static FILE*
wlroots_renderer_log()
{
  if constexpr (kWlrootsDebugLogs) {
    static FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    return f ? f : stderr;
  }
  return nullptr;
}

#define WL_RENDERER_LOG(...)                                                      \
  do {                                                                            \
    if (kWlrootsDebugLogs) {                                                      \
      FILE* f = wlroots_renderer_log();                                           \
      if (f) {                                                                    \
        std::fprintf(f, __VA_ARGS__);                                             \
        std::fflush(f);                                                           \
      }                                                                           \
    }                                                                             \
  } while (0)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <ctime>
#include <cmath>
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
  MESH_VERTEX.create();
  MESH_VERTEX_POSITIONS.create(GL_ARRAY_BUFFER);
  MESH_VERTEX_TEX_COORDS.create(GL_ARRAY_BUFFER);
  MESH_VERTEX_BLOCK_TYPES.create(GL_ARRAY_BUFFER);
  MESH_VERTEX_SELECTS.create(GL_ARRAY_BUFFER);
}

void
Renderer::genDynamicObjectResources()
{
  DYNAMIC_OBJECT_VERTEX.create();
  DYNAMIC_OBJECT_POSITIONS.create(GL_ARRAY_BUFFER);
};

void
Renderer::genGlResources()
{
  APP_VAO.create();
  APP_VBO.create(GL_ARRAY_BUFFER);

  DIRECT_RENDER_VAO.create();
  DIRECT_RENDER_VBO.create(GL_ARRAY_BUFFER);

  CURSOR_VAO.create();
  CURSOR_VBO.create(GL_ARRAY_BUFFER);

  LINE_VAO.create();
  LINE_VBO.create(GL_ARRAY_BUFFER);
  LINE_INSTANCE.create(GL_ARRAY_BUFFER);

  VOXEL_SELECTIONS.create();
  VOXEL_SELECTION_POSITIONS.create(GL_ARRAY_BUFFER);
  VOXEL_SELECTION_TEX_COORDS.create(GL_ARRAY_BUFFER);

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

  // software cursor quad (screen-space)
  glBindVertexArray(CURSOR_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, CURSOR_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
    1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
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
  , appIndexPool(IndexPool(8))
  , invertY(invertY)
  , appTexturesInitialized(false)
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
  // Align global screen dimensions to the actual GL viewport (Wayland context)
  // so default app sizing (Bootable defaults) matches the compositor output,
  // then rebuild the app quad with the correct aspect.
  auto refreshScreenFromViewport = [&]() {
    GLint vp[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] > 0 && vp[3] > 0) {
      SCREEN_WIDTH = static_cast<float>(vp[2]);
      SCREEN_HEIGHT = static_cast<float>(vp[3]);
      int defaultW =
        std::max(1, static_cast<int>(std::lround(SCREEN_WIDTH * 0.85f)));
      int defaultH =
        std::max(1, static_cast<int>(std::lround(SCREEN_HEIGHT * 0.85f)));
      Bootable::DEFAULT_WIDTH = defaultW;
      Bootable::DEFAULT_HEIGHT = defaultH;
      float newHeight = SCREEN_HEIGHT / SCREEN_WIDTH / 2.0f;
      float verts[] = {
        -0.5f, -newHeight, 0, 0.0f, 0.0f, 0.5f,  -newHeight, 0, 1.0f, 0.0f,
        0.5f,  newHeight,  0, 1.0f, 1.0f, 0.5f,  newHeight,  0, 1.0f, 1.0f,
        -0.5f, newHeight,  0, 0.0f, 1.0f, -0.5f, -newHeight, 0, 0.0f, 0.0f,
      };
      std::memcpy(appVertices, verts, sizeof(appVertices));
      glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
      glBufferData(
        GL_ARRAY_BUFFER, sizeof(appVertices), appVertices, GL_STATIC_DRAW);
      FILE* f = std::fopen("/tmp/matrix-debug.log", "a");
      if (f) {
        std::fprintf(f,
                     "Renderer: viewport=%d x %d -> SCREEN=%d x %d DEFAULT=%d x "
                     "%d newHeight=%.6f\n",
                     vp[2],
                     vp[3],
                     (int)SCREEN_WIDTH,
                     (int)SCREEN_HEIGHT,
                     defaultW,
                     defaultH,
                     newHeight);
        std::fflush(f);
        std::fclose(f);
      }
    }
  };
  refreshScreenFromViewport();
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
  cursorShader = new Shader("shaders/cursor.vert", "shaders/cursor.frag");

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
  if (appTexturesInitialized) {
    if constexpr (kWlrootsDebugLogs) {
      static FILE* logFile = []() {
        FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
        return f ? f : stderr;
      }();
      std::fprintf(logFile, "initAppTextures: already initialized, skipping\n");
      std::fflush(logFile);
    }
    return;
  }
  GLint maxUnits = 0;
  // Fragment shader texture units; use these to stay GLES-compatible.
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
  if (maxUnits <= 0) {
    maxUnits = 8;
  }
  // Keep a small pool to avoid exhausting units (GLES2 often has 8).
  appTextureCount = std::min(4, maxUnits);
  // Ensure pool starts at the same capacity it was constructed with to avoid
  // silently growing on re-init.
  appIndexPool = IndexPool(std::max(1, appTextureCount - 1));
  if constexpr (kWlrootsDebugLogs) {
    static FILE* logFile = []() {
      FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
      return f ? f : stderr;
    }();
    std::fprintf(logFile, "initAppTextures: maxUnits=%d appTextures=%d\n", maxUnits, appTextureCount);
  }
  for (int index = 0; index < appTextureCount; index++) {
    int textureN = index;
    int textureUnit = GL_TEXTURE0 + index; // dedicate a unit per app slot
    string textureName = "app" + to_string(index);
    textures[textureName] = new Texture(textureUnit);
    shader->setInt(textureName, index); // map sampler to its texture unit
    if constexpr (kWlrootsDebugLogs) {
      static FILE* logFile = []() {
        FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
        return f ? f : stderr;
      }();
      std::fprintf(logFile,
                   "initAppTextures: index=%d texName=%s texId=%u unit=%d\n",
                   index,
                   textureName.c_str(),
                   textures[textureName]->ID,
                   textureUnit - GL_TEXTURE0);
    }
  }
  if constexpr (kWlrootsDebugLogs) {
    static FILE* logFile = wlroots_renderer_log();
    std::fflush(logFile);
  }
  appTexturesInitialized = true;
}

void
Renderer::initCursorResources()
{
  if (cursorInitialized) {
    return;
  }
  // Generate a simple fractal tree into a small RGBA texture.
  constexpr int kCursorSize = 64;
  std::vector<uint32_t> pixels(kCursorSize * kCursorSize, 0x00000000);

  auto set_px = [&](int x, int y, uint32_t color) {
    if (x >= 0 && x < kCursorSize && y >= 0 && y < kCursorSize) {
      pixels[y * kCursorSize + x] = color;
    }
  };
  auto draw_line = [&](int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      set_px(x0, y0, color);
      if (x0 == x1 && y0 == y1) {
        break;
      }
      int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  };

  std::function<void(int, int, float, float, int)> draw_branch =
    [&](int x, int y, float angle, float length, int depth) {
      if (depth <= 0 || length < 2.0f) {
        return;
      }
      int x2 = x + static_cast<int>(std::cos(angle) * length);
      int y2 = y + static_cast<int>(std::sin(angle) * length);
      draw_line(x, y, x2, y2, 0xffffffff);
      float nextLen = length * 0.65f;
      float spread = 0.6f;
      draw_branch(x2, y2, angle - spread, nextLen, depth - 1);
      draw_branch(x2, y2, angle + spread, nextLen, depth - 1);
    };

  int baseX = kCursorSize / 2;
  int baseY = kCursorSize - 2;
  draw_branch(baseX, baseY, -glm::half_pi<float>(), static_cast<float>(kCursorSize) * 0.3f, 7);

  glGenTextures(1, &cursorTexture);
  glBindTexture(GL_TEXTURE_2D, cursorTexture);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               kCursorSize,
               kCursorSize,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  cursorShader->use();
  cursorShader->setInt("uTexture", 0);
  cursorInitialized = true;
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

  auto logRenderAppDirect = [&](const char* status, int destX, int destY) {
    FILE* rlog = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    if (!rlog) {
      return;
    }
    std::fprintf(rlog,
                 "Renderer: renderAppDirect status=%s appNumber=%d texId=%d texUnit=%d dest=(%d,%d) size=%dx%d direct=1 bootable=%d\n",
                 status,
                 index,
                 app->getTextureId(),
                 app->getTextureUnit() - GL_TEXTURE0,
                 destX,
                 destY,
                 appWidth,
                 appHeight,
                 bootable ? 1 : 0);
    std::fflush(rlog);
    std::fclose(rlog);
  };

  if (fbIt == frameBuffers.end()) {
    logRenderAppDirect("missing-fbo", pos[0], pos[1]);
    return;
  }

  if (index >= 0) {
    if (!bootable) {
      GLint prevReadFbo = 0;
      GLint prevDrawFbo = 0;
      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
      logRenderAppDirect("blit", pos[0], pos[1]);
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
      glActiveTexture(GL_TEXTURE0 + index);
      auto texIt = textures.find("app" + std::to_string(index));
      if (texIt != textures.end()) {
        glBindTexture(GL_TEXTURE_2D, texIt->second->ID);
      }
      shader->setInt("appNumber", index);
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
      logRenderAppDirect("bootable", bootable->x, bootable->y);
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
Renderer::renderSoftwareCursor(float xPixels, float yPixels, float sizePixels)
{
  initCursorResources();
  cursorShader->use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, cursorTexture);

  // Flip Y from top-left pixel coords (Wayland) into GL clip space.
  glm::vec2 mapped = mapCursorToScreen(xPixels, yPixels);
  float yFlipped = SCREEN_HEIGHT - mapped.y - sizePixels;
  float left = static_cast<float>(mapped.x) / SCREEN_WIDTH * 2.0f - 1.0f;
  float right =
    static_cast<float>(mapped.x + sizePixels) / SCREEN_WIDTH * 2.0f - 1.0f;
  float top = 1.0f - static_cast<float>(yFlipped) / SCREEN_HEIGHT * 2.0f;
  float bottom =
    1.0f - static_cast<float>(yFlipped + sizePixels) / SCREEN_HEIGHT * 2.0f;

  float verts[] = {
    left,  bottom, 0.0f, 0.0f, 1.0f, right, bottom, 0.0f, 1.0f, 1.0f,
    right, top,    0.0f, 1.0f, 0.0f, left,  top,    0.0f, 0.0f, 0.0f,
    left,  bottom, 0.0f, 0.0f, 1.0f, right, top,    0.0f, 1.0f, 0.0f
  };

  glBindVertexArray(CURSOR_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, CURSOR_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_TEST);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  if (depthEnabled) {
    glEnable(GL_DEPTH_TEST);
  }
}

glm::vec2
Renderer::mapCursorToScreen(float xPixels, float yPixels) const
{
  // When the rendered app viewport is centered in the output, shift the cursor
  // so it lines up with the drawn content.
  float offsetX = std::max(0.0f, (SCREEN_WIDTH - Bootable::DEFAULT_WIDTH) * 0.5f);
  float offsetY = std::max(0.0f, (SCREEN_HEIGHT - Bootable::DEFAULT_HEIGHT) * 0.5f);
  return glm::vec2(xPixels + offsetX, yPixels + offsetY);
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
  auto lookedAtAppEntity = windowManagerSpace->getLookedAtApp();
  // Unconditional logging here to diagnose Wayland focus/unfocus draw issues.
  static FILE* logFile = []() {
    FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
    return f ? f : stderr;
  }();
  static std::unordered_set<void*> loggedApps;
  auto wlPositionable =
    registry->view<WaylandApp::Component, Positionable>(entt::exclude<Bootable>);
  size_t wlCount = wlPositionable.size_hint();
  if (logFile) {
    int focusedEnt = -1;
    if (wm && wm->getCurrentlyFocusedApp().has_value()) {
      focusedEnt = (int)entt::to_integral(wm->getCurrentlyFocusedApp().value());
    }
    int lookedEnt = -1;
    if (lookedAtAppEntity.has_value()) {
      lookedEnt = (int)entt::to_integral(lookedAtAppEntity.value());
    }
    std::fprintf(logFile,
                 "renderer: frame start focused=%d looked=%d\n",
                 focusedEnt,
                 lookedEnt);
    GLint vaoBound = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBound);
    GLint fboBound = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fboBound);
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    std::fprintf(logFile,
                 "renderer: vaoBound=%d fbo=%d program=%d countWayland=%zu\n",
                 vaoBound,
                 fboBound,
                 prog,
                 wlCount);
    std::fflush(logFile);
  }
  TracyGpuZone("render apps");
  shader->setBool("appSelected", false);

  shader->setBool("isApp", true);
  shader->setBool("directRender", false);
  glBindVertexArray(APP_VAO);
  // Defensive: ensure app quad attributes are enabled/bound even if VAO state
  // was clobbered by other GL paths.
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
    1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glDisable(GL_CULL_FACE);
  if (logFile) {
    GLint vaoBound = 0;
    GLint arrayBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBound);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    auto logAttribState = [&](int idx) {
      GLint enabled = 0;
      GLint buf = 0;
      void* ptr = nullptr;
      glGetVertexAttribiv(idx, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
      glGetVertexAttribiv(idx, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buf);
      glGetVertexAttribPointerv(idx, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
      std::fprintf(logFile,
                   "renderer: attrib%d enabled=%d buf=%d ptr=%p\n",
                   idx,
                   enabled,
                   buf,
                   ptr);
    };
    std::fprintf(logFile,
                 "renderer: vao state pre-apps vaoBound=%d arrayBuffer=%d\n",
                 vaoBound,
                 arrayBuffer);
    logAttribState(0);
    logAttribState(1);
    std::fflush(logFile);
  }
  auto bindAppTexture = [&](int index) {
    auto it = textures.find("app" + std::to_string(index));
    if (index < 0 || index >= appTextureCount ||
        it == textures.end() || !it->second || !glIsTexture(it->second->ID)) {
      if (logFile) {
        std::fprintf(logFile,
                     "renderer: bindAppTexture skip idx=%d (missing/invalid)\n",
                     index);
        std::fflush(logFile);
      }
      return false;
    }
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, it->second->ID);
    shader->setInt("appNumber", index);
    if (logFile) {
      std::fprintf(logFile,
                   "renderer: bindAppTexture idx=%d texId=%d\n",
                   index,
                   it->second->ID);
      std::fflush(logFile);
    }
    return true;
  };
  auto renderWaylandPopup = [&](WaylandApp::Component& popup,
                                WaylandApp::Component& parent) {
    auto* popupApp = popup.app.get();
    auto* parentApp = parent.app.get();
    if (!popupApp || !parentApp) {
      return;
    }
    int popupW = popupApp->getWidth();
    int popupH = popupApp->getHeight();
    int parentW = parentApp->getWidth();
    int parentH = parentApp->getHeight();
    if (popupW <= 0 || popupH <= 0 || parentW <= 0 || parentH <= 0) {
      return;
    }
    float parentLeft = std::max(0.0f, (SCREEN_WIDTH - parentW) * 0.5f);
    float parentTop = std::max(0.0f, (SCREEN_HEIGHT - parentH) * 0.5f);
    int destX = static_cast<int>(parentLeft + popup.offset_x);
    int destY =
      static_cast<int>(SCREEN_HEIGHT - (parentTop + popup.offset_y) - popupH);

    if (popupApp->needsTextureImport()) {
      popupApp->appTexture();
    }
    int idx = static_cast<int>(popupApp->getAppIndex());
    auto fbIt = frameBuffers.find(idx);
    if (fbIt == frameBuffers.end()) {
      return;
    }
    GLint prevReadFbo = 0;
    GLint prevDrawFbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbIt->second);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
    glBlitFramebuffer(0,
                      popupH,
                      popupW,
                      0,
                      destX,
                      destY,
                      destX + popupW,
                      destY + popupH,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
  };
  auto renderLayerShell = [&](WaylandApp::Component& comp) {
    auto* app = comp.app.get();
    if (!app) {
      return;
    }
    int w = app->getWidth();
    int h = app->getHeight();
    if (w <= 0 || h <= 0) {
      return;
    }
    if (app->needsTextureImport()) {
      app->appTexture();
    }
    int idx = static_cast<int>(app->getAppIndex());
    auto fbIt = frameBuffers.find(idx);
    if (fbIt == frameBuffers.end()) {
      return;
    }
    GLint prevReadFbo = 0;
    GLint prevDrawFbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbIt->second);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
    int destX = comp.screen_x;
    int destY = static_cast<int>(SCREEN_HEIGHT - comp.screen_y - h);
    glBlitFramebuffer(0,
                      h,
                      w,
                      0,
                      destX,
                      destY,
                      destX + w,
                      destY + h,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
  };

  // this is LEGACY for msedge
  auto positionableNonBootable =
    registry->view<X11App, Positionable>(entt::exclude<Bootable>);
  for (auto [entity, app, positionable] : positionableNonBootable.each()) {
    int idx = static_cast<int>(app.getAppIndex());
    if (!bindAppTexture(idx)) {
      continue;
    }
    shader->setMatrix4("model", positionable.modelMatrix);

    // OPTIMIZATION
    // TODO: cache the recomputeHeightScaler into the app itself and don't recompute it every render
    shader->setMatrix4("bootableScale", app.getHeightScalar());
    shader->setBool("appTransparent", false);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  auto positionableApps = registry->view<X11App, Positionable, Bootable>();
  for (auto [entity, app, positionable, bootable] : positionableApps.each()) {
    int idx = static_cast<int>(app.getAppIndex());
    if (!bindAppTexture(idx)) {
      continue;
    }
    shader->setMatrix4("model", positionable.modelMatrix);
    shader->setMatrix4("bootableScale", bootable.getHeightScaler());
    if (app.isSelected()) {
      shader->setBool("appSelected", true);
    }
    if (bootable.transparent) {
      shader->setBool("appTransparent", true);
    } else {
      shader->setBool("appTransparent", false);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef ENABLE_RENDER_TMP_LOGS
    if (loggedApps.insert((void*)&app).second) {
      std::fprintf(logFile,
                   "renderer: first render X11 app idx=%zu size=%dx%d\n",
                   app.getAppIndex(),
                   bootable.getWidth(),
                   bootable.getHeight());
      std::fflush(logFile);
    }
#endif
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
    if (!bindAppTexture(static_cast<int>(directApp.getAppIndex()))) {
      continue;
    }
    drawAppDirect(&directApp);
  }

  auto directRenderNonBlits =
    registry->view<X11App, Bootable>(entt::exclude<Positionable>);
  for (auto [entity, directApp, bootable] : directRenderNonBlits.each()) {
    if (!bindAppTexture(static_cast<int>(directApp.getAppIndex()))) {
      continue;
    }
    drawAppDirect(&directApp, &bootable);
  }

  // Wayland apps: render any that were registered by the wlroots backend.
  for (auto [entity, comp, positionable] : wlPositionable.each()) {
    auto* app = comp.app.get();
    if (!app) {
      continue;
    }
#ifdef ENABLE_RENDER_TMP_LOGS
    static FILE* rlog = []() {
      FILE* f = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
      return f ? f : stderr;
    }();
    if (rlog) {
      std::fprintf(rlog,
                   "renderer: wl draw ent=%d texId=%d texUnit=%d size=%dx%d direct=%d model=[%.2f %.2f %.2f; %.2f %.2f %.2f; %.2f %.2f %.2f; %.2f %.2f %.2f]\n",
                   (int)entity,
                   app->getTextureId(),
                   app->getTextureUnit() - GL_TEXTURE0,
                   app->getWidth(),
                   app->getHeight(),
                   (wm && wm->getCurrentlyFocusedApp().has_value() &&
                    wm->getCurrentlyFocusedApp().value() == entity),
                   positionable.modelMatrix[0][0],
                   positionable.modelMatrix[0][1],
                   positionable.modelMatrix[0][2],
                   positionable.modelMatrix[1][0],
                   positionable.modelMatrix[1][1],
                   positionable.modelMatrix[1][2],
                   positionable.modelMatrix[2][0],
                   positionable.modelMatrix[2][1],
                   positionable.modelMatrix[2][2],
                   positionable.modelMatrix[3][0],
                   positionable.modelMatrix[3][1],
                   positionable.modelMatrix[3][2]);
      std::fflush(rlog);
    }
#endif
    int idx = static_cast<int>(app->getAppIndex());
    // Upload latest buffer only when a new commit arrived, then bind to the
    // app's dedicated unit to avoid stale or shared textures when multiple
    // Wayland apps are present.
    if (app->needsTextureImport()) {
      app->appTexture();
    }
    if (!bindAppTexture(idx)) {
      continue;
    }
    // Keep the in-world quad at the positionable's size; just track the app's
    // pixel size for logging and aspect ratio.
    float sx = static_cast<float>(app->getWidth()) /
               static_cast<float>(SCREEN_WIDTH);
    float sy = static_cast<float>(app->getHeight()) /
               static_cast<float>(SCREEN_HEIGHT);
    glm::mat4 model = positionable.modelMatrix;
    shader->setMatrix4("model", model);
    shader->setMatrix4("bootableScale", app->getHeightScalar());
    shader->setInt("appNumber", idx);
    shader->setBool("appTransparent", false);
    if (logFile) {
      bool isFocused =
        wm && wm->getCurrentlyFocusedApp().has_value() &&
        wm->getCurrentlyFocusedApp().value() == entity;
      bool isLooked = lookedAtAppEntity.has_value() && lookedAtAppEntity.value() == entity;
      GLboolean texValid = glIsTexture(app->getTextureId());
      glm::vec4 center = model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      glm::vec4 clip = camera->getProjectionMatrix(true) *
                       camera->getViewMatrix() * center;
      float ndcX = clip.w != 0.0f ? clip.x / clip.w : 0.0f;
      float ndcY = clip.w != 0.0f ? clip.y / clip.w : 0.0f;
      float ndcZ = clip.w != 0.0f ? clip.z / clip.w : 0.0f;
      std::fprintf(logFile,
                   "renderer: wl draw ent=%d appIdx=%d focused=%d looked=%d texId=%d texUnit=%d modelPos=(%.2f,%.2f,%.2f) modelRow0=(%.2f,%.2f,%.2f,%.2f) modelRow1=(%.2f,%.2f,%.2f,%.2f) modelRow2=(%.2f,%.2f,%.2f,%.2f) modelRow3=(%.2f,%.2f,%.2f,%.2f) scale=(%.3f,%.3f) texValid=%d center=(%.2f,%.2f,%.2f,%.2f) ndc=(%.2f,%.2f,%.2f)\n",
                   (int)entity,
                   idx,
                   isFocused ? 1 : 0,
                   isLooked ? 1 : 0,
                   app->getTextureId(),
                   app->getTextureUnit() - GL_TEXTURE0,
                   positionable.modelMatrix[3][0],
                   positionable.modelMatrix[3][1],
                   positionable.modelMatrix[3][2],
                   model[0][0],
                   model[0][1],
                   model[0][2],
                   model[0][3],
                   model[1][0],
                   model[1][1],
                   model[1][2],
                   model[1][3],
                   model[2][0],
                   model[2][1],
                   model[2][2],
                   model[2][3],
                   model[3][0],
                   model[3][1],
                   model[3][2],
                   model[3][3],
                   sx,
                   sy,
                   texValid,
                   center.x,
                   center.y,
                   center.z,
                   center.w,
                   ndcX,
                   ndcY,
                   ndcZ);
      std::fflush(logFile);
    }
#ifdef ENABLE_RENDER_TMP_LOGS
    std::fprintf(logFile,
                 "Renderer: Wayland in-world ent=%d appNumber=%zu texId=%d texUnit=%d size=%dx%d screenScale=(%.3f,%.3f)\n",
                 (int)entity,
                 app->getAppIndex(),
                 app->getTextureId(),
                 app->getTextureUnit() - GL_TEXTURE0,
                 app->getWidth(),
                 app->getHeight(),
                 sx,
                 sy);
#endif
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (logFile) {
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
        std::fprintf(logFile, "renderer: gl error after wl draw ent=%d err=0x%x\n", (int)entity, err);
        std::fflush(logFile);
      }
    }
    // If focused, also draw directly to screen to ensure visibility.
    if (wm && wm->getCurrentlyFocusedApp().has_value() &&
        wm->getCurrentlyFocusedApp().value() == entity) {
      if (!bindAppTexture(idx)) {
        continue;
      }
      shader->setBool("directRender", true);
      shader->setInt("appNumber", idx);
      glm::mat4 model = glm::mat4(1.0f);
      float sx = static_cast<float>(app->getWidth()) /
                 static_cast<float>(SCREEN_WIDTH);
      float sy = static_cast<float>(app->getHeight()) /
                 static_cast<float>(SCREEN_HEIGHT);
      model = glm::scale(model, glm::vec3(sx, sy, 1.0f));
      shader->setMatrix4("model", model);
#ifdef ENABLE_RENDER_TMP_LOGS
      std::fprintf(logFile,
                   "Renderer: Wayland direct ent=%d appNumber=%zu texId=%d texUnit=%d screenScale=(%.3f,%.3f)\n",
                   (int)entity,
                   app->getAppIndex(),
                   app->getTextureId(),
                   app->getTextureUnit() - GL_TEXTURE0,
                   sx,
                   sy);
#endif
      // Always emit a direct-render marker for tests.
      {
        FILE* drlog = std::fopen("/tmp/matrix-wlroots-renderer.log", "a");
        if (drlog) {
          std::fprintf(drlog,
                       "Renderer: Wayland direct ent=%d appNumber=%zu texId=%d texUnit=%d screenScale=(%.3f,%.3f)\n",
                       (int)entity,
                       app->getAppIndex(),
                       app->getTextureId(),
                       app->getTextureUnit() - GL_TEXTURE0,
                       sx,
                       sy);
          std::fflush(drlog);
          std::fclose(drlog);
        }
      }
      glBindVertexArray(DIRECT_RENDER_VAO);
      GLboolean depthMask = GL_TRUE;
      glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE); // avoid clobbering depth for in-world apps
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glDepthMask(depthMask);
      glEnable(GL_DEPTH_TEST);
      shader->setBool("directRender", false);
      // Restore model matrix/VAO for subsequent in-world draws.
      shader->setMatrix4("model", positionable.modelMatrix);
      glBindVertexArray(APP_VAO);
#ifdef ENABLE_RENDER_TMP_LOGS
      if (logFile) {
        GLint vaoBound = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBound);
        std::fprintf(logFile,
                     "renderer: post-direct restore ent=%d vaoBound=%d\n",
                     (int)entity,
                     vaoBound);
        std::fflush(logFile);
      }
#endif
    }
    if (logFile && loggedApps.insert((void*)app).second) {
      std::fprintf(logFile,
                   "renderer: first render Wayland app size=%dx%d texId=%d unit=%d\n",
                   app->getWidth(),
                   app->getHeight(),
                   app->getTextureId(),
                   app->getTextureUnit() - GL_TEXTURE0);
      std::fflush(logFile);
    }
  }

  // Direct-render Wayland accessory apps (popups/menus) relative to their
  // parent surface so they land at the correct on-screen position.
  if (wm && registry) {
    auto focused = wm->getCurrentlyFocusedApp();
    entt::entity focusedEnt =
      focused.has_value() ? focused.value() : entt::null;
    auto layerShells =
      registry->view<WaylandApp::Component>(entt::exclude<Positionable>);
    for (auto [ent, comp] : layerShells.each()) {
      if (!comp.layer_shell) {
        continue;
      }
      renderLayerShell(comp);
    }
    if (focusedEnt != entt::null) {
      auto accessories =
        registry->view<WaylandApp::Component>(entt::exclude<Positionable>);
      for (auto [ent, comp] : accessories.each()) {
        if (!comp.accessory) {
          continue;
        }
        if (comp.layer_shell) {
          continue;
        }
        if (comp.parent == entt::null || comp.parent != focusedEnt) {
          continue;
        }
        if (!registry->valid(comp.parent)) {
          continue;
        }
        auto* parentComp = registry->try_get<WaylandApp::Component>(comp.parent);
        if (!parentComp || !parentComp->app) {
          continue;
        }
        renderWaylandPopup(comp, *parentComp);
      }
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
  auto allWl = registry->view<WaylandApp::Component, Positionable>();
  size_t wlCount = allWl.size_hint();
  WL_RENDERER_LOG("Renderer: frame WaylandApp count=%zu\n", wlCount);
  allWl.each([&](auto ent, WaylandApp::Component& comp, Positionable& pos) {
    WL_RENDERER_LOG("Renderer: Wayland app ent=%d pos=(%.2f,%.2f,%.2f) texId=%d surface=%p\n",
                    (int)entt::to_integral(ent),
                    pos.pos.x,
                    pos.pos.y,
                    pos.pos.z,
                    comp.app ? comp.app->getTextureId() : -1,
                    comp.app ? (void*)comp.app->getSurface() : nullptr);
  });
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
  if (index < 0) {
    WL_RENDERER_LOG("registerApp: no available texture slots; using shared slot 0\n");
    attachSharedAppTexture(app);
    return;
  }
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
  FILE* logFile = kWlrootsDebugLogs ? wlroots_renderer_log() : nullptr;
  try {
    int textureId = textures["app" + to_string(index)]->ID;
    int textureUnit = GL_TEXTURE0 + index;
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
    }
    appIndexPool.relinquishIndex(index);
    throw;
  }
  if (logFile) {
    std::fflush(logFile);
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
  if (kWlrootsDebugLogs) {
    FILE* logFile2 = wlroots_renderer_log();
    if (logFile2) {
      std::fprintf(logFile2,
                   "registerApp: framebuffer %u attached to texId=%u index=%u\n",
                   framebufferId,
                   textures["app" + to_string(index)]->ID,
                   static_cast<unsigned>(index));
      std::fflush(logFile2);
    }
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
  if (kWlrootsDebugLogs) {
    FILE* logFile = wlroots_renderer_log();
    if (logFile) {
      std::fprintf(logFile,
                   "attachSharedAppTexture: texId=%d unit=%d\n",
                   textureId,
                   textureUnit - GL_TEXTURE0);
      std::fflush(logFile);
    }
  }
}

void
Renderer::toggleMeshing()
{
  logger->debug("toggleMeshing");
  logger->flush();
}

void
Renderer::wireWindowManager(WindowManager::WindowManagerPtr wm,
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
