#include "IndexPool.h"
#include "glm/ext/matrix_transform.hpp"
#include "model.h"
#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include "camera.h"
#include "app.h"
#include "cube.h"
#include "components/Bootable.h"
#include <vector>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <ctime>
#include <iomanip>

float HEIGHT = 0.27;

float appVertices[] = {
  -0.5f, -HEIGHT, 0, 0.0f, 0.0f,
  0.5f, -HEIGHT, 0, 1.0f, 0.0f,
  0.5f, HEIGHT, 0, 1.0f, 1.0f,
  0.5f, HEIGHT, 0, 1.0f, 1.0f,
  -0.5f, HEIGHT, 0, 0.0f, 1.0f,
  -0.5f, -HEIGHT, 0, 0.0f, 0.0f,
};

void Renderer::genMeshResources() {
  glGenVertexArrays(1, &MESH_VERTEX);
  glGenBuffers(1, &MESH_VERTEX_POSITIONS);
  glGenBuffers(1, &MESH_VERTEX_TEX_COORDS);
  glGenBuffers(1, &MESH_VERTEX_BLOCK_TYPES);
  glGenBuffers(1, &MESH_VERTEX_SELECTS);
}

void Renderer::genDynamicObjectResources() {
  glGenVertexArrays(1, &DYNAMIC_OBJECT_VERTEX);
  glGenBuffers(1, &DYNAMIC_OBJECT_POSITIONS);
};

void Renderer::genGlResources() {
  glGenVertexArrays(1, &APP_VAO);
  glGenBuffers(1, &APP_VBO);

  glGenVertexArrays(1, &LINE_VAO);
  glGenBuffers(1, &LINE_VBO);
  glGenBuffers(1, &LINE_INSTANCE);

  glGenVertexArrays(1, &VOXEL_SELECTIONS);
  glGenBuffers(1, &VOXEL_SELECTION_POSITIONS);
  glGenBuffers(1, &VOXEL_SELECTION_TEX_COORDS);

  genMeshResources();
  genDynamicObjectResources();
}

void Renderer::setupMeshVertexAttributePoiners() {
  glBindVertexArray(MESH_VERTEX);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
  glVertexAttribIPointer(2, 1, GL_INT, sizeof(int), (void *)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
  glVertexAttribIPointer(3, 1, GL_INT, sizeof(int), (void *)0);
  glEnableVertexAttribArray(3);


}

void Renderer::setupDynamicObjectVertexAttributePointers() {
  glBindVertexArray(DYNAMIC_OBJECT_VERTEX);
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
}

void Renderer::setupVertexAttributePointers() {
  setupMeshVertexAttributePoiners();
  setupDynamicObjectVertexAttributePointers();

  glBindVertexArray(APP_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // line
  glBindVertexArray(LINE_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);

  glBindVertexArray(VOXEL_SELECTIONS);
  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);

}

int MAX_CUBES = 1000000;

void Renderer::fillDynamicObjectBuffers() {
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 30000), (void *)0, GL_DYNAMIC_DRAW);
}

void Renderer::fillBuffers() {
   glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(appVertices), appVertices, GL_STATIC_DRAW);


  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void *)0,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void *)0,
               GL_STATIC_DRAW);


  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 36 * MAX_CUBES), (void *) 0, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec2) * 36 * MAX_CUBES), (void *)0,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(int) * 36 * MAX_CUBES), (void *)0,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(int) * 36 * MAX_CUBES), (void *)0,
               GL_DYNAMIC_DRAW);


  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 6), (void *)0,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec2) * 6), (void *)0,
               GL_DYNAMIC_DRAW);

  fillDynamicObjectBuffers();
}

void Renderer::toggleWireframe() {
  if (isWireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
    isWireframe = false;
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
    isWireframe = true;
  }
}

Renderer::Renderer(shared_ptr<EntityRegistry> registry, Camera *camera, World *world, shared_ptr<blocks::TexturePack> texturePack):
  texturePack(texturePack),
  registry(registry),
  appIndexPool(IndexPool(10))
{
  this->camera = camera;
  this->world = world;

  logger = make_shared<spdlog::logger>("Renderer", fileSink);
  logger->set_level(spdlog::level::debug);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //glEnable(GL_MULTISAMPLE);
  genGlResources();
  fillBuffers();
  setupVertexAttributePointers();

  /*
  std::vector<std::string> images = {
      "images/bAndGrey.png",         "images/purpleRoad.png",
      "images/bAndGreySpeckled.png", "images/grass.png",
      "images/pillar.png",           "images/reactor_texture.png",
      "images/road.png"};
  */

  std::vector<std::string> images = texturePack->imageNames();
  textures.insert(std::pair<string, Texture *>(
      "allBlocks", new Texture(images, GL_TEXTURE0)));
  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shader->use(); // may need to move into loop to use changing uniforms

  shader->setInt("allBlocks", 0);
  shader->setInt("totalBlockTypes", images.size());

  shader->setVec3("lightPos", glm::vec3(0, 0, 0));
  shader->setVec3("lightColor", glm::vec3(0.5, 0.5, 0.5));

  initAppTextures();

  shader->setBool("lookedAtValid", false);
  shader->setBool("isLookedAt", false);
  shader->setBool("isMesh", false);
  shader->setBool("isModel", false);

  if(isWireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
  }
  glClearColor(163.0/255.0, 163.0/255.0, 167.0/255.0, 1.0f);
  glLineWidth(10.0);

  view = view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
  projection =
      glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.02f, 100.0f);
  meshModel = glm::scale(glm::mat4(1.0f), glm::vec3(world->CUBE_SIZE));
}

void Renderer::initAppTextures() {
  for (int index = 0; index < 20; index++) {
    int textureN = 31 - index;
    int textureUnit = GL_TEXTURE0 + textureN;
    string textureName = "app" + to_string(index);
    textures.insert(
        std::pair<string, Texture *>(textureName, new Texture(textureUnit)));
    shader->setInt(textureName, textureN);
  }
}

void Renderer::updateTransformMatrices() {
  unsigned int modelLoc = glGetUniformLocation(shader->ID, "meshModel");
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(meshModel));

  unsigned int viewLoc = glGetUniformLocation(shader->ID, "view");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  unsigned int projectionLoc = glGetUniformLocation(shader->ID, "projection");
  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void Renderer::updateChunkMeshBuffers(vector<shared_ptr<ChunkMesh>> &meshes) {
  verticesInMesh = 0;
  for(auto mesh: meshes) {
    //if(mesh.updated) {
      glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_POSITIONS);
      glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * verticesInMesh,
                      sizeof(glm::vec3) * mesh->positions.size(),
                      mesh->positions.data());

      glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_TEX_COORDS);
      glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * verticesInMesh,
                      sizeof(glm::vec2) * mesh->texCoords.size(),
                      mesh->texCoords.data());

      glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_BLOCK_TYPES);
      glBufferSubData(GL_ARRAY_BUFFER, sizeof(int) * verticesInMesh,
                      sizeof(int) * mesh->blockTypes.size(),
                      mesh->blockTypes.data());

      glBindBuffer(GL_ARRAY_BUFFER, MESH_VERTEX_SELECTS);
      glBufferSubData(GL_ARRAY_BUFFER, sizeof(int) * verticesInMesh,
                      sizeof(int) * mesh->selects.size(),
                      mesh->selects.data());
      //}
    verticesInMesh += mesh->positions.size();
  }
}

void Renderer::updateDynamicObjects(shared_ptr<DynamicObject> obj) {
  auto renderable = obj->makeRenderable();
  glBindBuffer(GL_ARRAY_BUFFER, DYNAMIC_OBJECT_POSITIONS);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * renderable.vertices.size(),
                  renderable.vertices.data());
  verticesInDynamicObjects = renderable.vertices.size();
}

void Renderer::addLine(int index, Line line) {
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2) * index,
                  sizeof(glm::vec3), &line.points[0]);
  glBufferSubData(GL_ARRAY_BUFFER,
                  (sizeof(glm::vec3) * 2) * index + (sizeof(glm::vec3)),
                  sizeof(glm::vec3), &line.points[1]);

  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glBufferSubData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 2 * index),
                  (sizeof(glm::vec3)), &line.color);
  glBufferSubData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 2 * index + sizeof(glm::vec3)),
                  (sizeof(glm::vec3)), &line.color);
}

void Renderer::renderDynamicObjects() {
  glBindVertexArray(DYNAMIC_OBJECT_VERTEX);
  shader->setBool("isDynamicObject", true);
  glDrawArrays(GL_TRIANGLES, 0, verticesInDynamicObjects);
  shader->setBool("isDynamicObject", false);
}

void Renderer::drawAppDirect(X11App *app) {
  int index = app->getAppIndex();
  int screenWidth = 1920;
  int screenHeight = 1080;
  int appWidth = app->width;
  int appHeight = app->height;
  auto pos = app->getPosition();
  if (index >= 0) {
    glBlitNamedFramebuffer(frameBuffers[index], 0,
                           // src x1, src y1 (flip)
                           0, appHeight,
                           // end x2, end y2 (flip)
                           appWidth, 0,

                           // dest x1,y1,x2,y2
                           /*
                           (screenWidth - appWidth) / 2,
                           (screenHeight - appHeight) / 2,
                           appWidth + (screenWidth - appWidth) / 2,
                           appHeight + (screenHeight - appHeight) / 2,
                           */
                           pos[0],
                           pos[1],
                           pos[0] + appWidth,
                           pos[1] + appHeight,


                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
}

void Renderer::screenshot() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  stbi_flip_vertically_on_write(true);
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  stringstream filenameSS;
  filenameSS << "screenshots/" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S.png");

  string filename = filenameSS.str();

  // Capture the screenshot and save it as a PNG file
  int width = 1920;  // Width of your rendering area
  int height = 1080; // Height of your rendering area
  int channels = 4;  // 4 for RGBA
  unsigned char *data = new unsigned char[width * height * channels];
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
  std::thread saver([filename, width, height, channels, data]() {
    stbi_write_png(filename.c_str(), width, height, channels, data,
                   width * channels);
    delete[] data;
  });
  saver.detach();
}

void Renderer::renderLookedAtFace() {
  Position lookedAt = world->getLookedAtCube();
  if (lookedAt.valid) {
    ChunkMesh lookedAtFace = world->meshSelectedCube(lookedAt);

    glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_POSITIONS);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 6 * (sizeof(glm::vec3)),
                    lookedAtFace.positions.data());

    glBindBuffer(GL_ARRAY_BUFFER, VOXEL_SELECTION_TEX_COORDS);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 6 * (sizeof(glm::vec2)),
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

void Renderer::updateShaderUniforms() {
  shader->setFloat("time", glfwGetTime());
  shader->setBool("isApp", false);
  shader->setBool("isLine", false);

  updateTransformMatrices();
}


void Renderer::renderChunkMesh() {
  shader->setBool("isMesh", true);
  glBindVertexArray(MESH_VERTEX);
  // TODO: fix this
  //glEnable(GL_CULL_FACE);
  glDisable(GL_CULL_FACE);
  glDrawArrays(GL_TRIANGLES, 0, verticesInMesh);
  shader->setBool("isMesh", false);
}

void Renderer::renderApps() {
  auto appEntity = windowManagerSpace->getLookedAtApp();
  shader->setBool("appSelected", false);

  shader->setBool("isApp", true);
  glBindVertexArray(APP_VAO);
  glDisable(GL_CULL_FACE);
  auto positionableApps = registry->view<X11App, Positionable>();
  for(auto [entity, app, positionable]: positionableApps.each()) {
    auto bootable = registry->try_get<Bootable>(entity);
    shader->setMatrix4("model", positionable.modelMatrix);
    if(bootable) {
      shader->setMatrix4("bootableScale", bootable->getHeightScaler());
    } else {
      shader->setMatrix4("bootableScale", glm::mat4(1.0));
    }
    shader->setInt("appNumber", app.getAppIndex());
    if(bootable && bootable->transparent) {
      shader->setBool("appTransparent", true);
    } else {
      shader->setBool("appTransparent", false);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  if (appEntity.has_value()) {
    auto bootable = registry->try_get<Bootable>(appEntity.value());
    auto &app = registry->get<X11App>(appEntity.value());
    if(app.isFocused() && (!bootable || !bootable->transparent)) {
      shader->setBool("appSelected", app.isFocused());
      drawAppDirect(&app);
      shader->setBool("appSelected", false);
    }
  }

  auto directRenders = registry->view<X11App>(entt::exclude<Positionable>);
  for (auto [entity, directApp] : directRenders.each()) {
    drawAppDirect(&directApp);
  }

  shader->setBool("isApp", false);
}

void Renderer::renderLines() {
  shader->setBool("isLine", true);
  glBindVertexArray(LINE_VAO);
  glDrawArrays(GL_LINES, 0, world->getLines().size()*2);
  shader->setBool("isLine", false);
}

void Renderer::renderModels() {
  shader->setBool("isModel", true);
  shader->setVec3("viewPos", camera->position);
  shader->setBool("isLight", false);
  auto modelView = registry->view<Positionable, Model>();
  auto lightView = registry->view<Light,Positionable>();


  entt::entity lightEntity;
  bool hasLight = false;

  for(auto [entity, light, positionable]: lightView.each()) {
    shader->setVec3("lightPos", positionable.pos);
    shader->setVec3("lightColor", light.color);
    lightEntity = entity;
    hasLight = true;
  }

  for(auto [entity, p, m]: modelView.each()) {
    if(hasLight && lightEntity == entity) {
      shader->setBool("isLight", true);
    }
    shader->setMatrix3("normalMatrix", p.normalMatrix);
    shader->setMatrix4("model", p.modelMatrix);

    m.Draw(*shader);
    shader->setBool("isLight", false);
  }
  shader->setBool("isModel", false);
}

void Renderer::render() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  view = camera->tick();
  updateShaderUniforms();
  renderLines();
  renderLookedAtFace();
  renderChunkMesh();
  renderDynamicObjects();
  renderModels();
  renderApps();
}

Camera *Renderer::getCamera() { return camera; }

void Renderer::registerApp(X11App *app) {
  // We need to keep track of which textureN has been used.
  // because deletions means this won't work
  // indices will change.
  auto index = appIndexPool.acquireIndex();
  try {
    int textureN = 31 - index;
    int textureUnit = GL_TEXTURE0 + textureN;
    int textureId = textures["app" + to_string(index)]->ID;
    app->attachTexture(textureUnit, textureId, index);
    app->appTexture();
  } catch (...) {
    appIndexPool.relinquishIndex(index);
    throw;
  }

  unsigned int framebufferId;
  glGenFramebuffers(1, &framebufferId);
  frameBuffers[index] = framebufferId;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferId);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, textures["app" + to_string(index)]->ID,
                         0);
}

void Renderer::deregisterApp(int index) {
  appIndexPool.relinquishIndex(index);
  glDeleteFramebuffers(1, &frameBuffers[index]);
  frameBuffers.erase(index);
}

void Renderer::toggleMeshing() {
  logger->debug("toggleMeshing");
  logger->flush();
}

void Renderer::wireWindowManagerSpace(shared_ptr<WindowManager::Space> windowManagerSpace) {
  this->windowManagerSpace = windowManagerSpace;
}

Renderer::~Renderer() {
  delete shader;
  for (auto &t : textures) {
    delete t.second;
  }
}

