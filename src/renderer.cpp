#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include "camera.h"
#include "app.h"
#include "cube.h"
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

float vertices[] = {
  -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
  0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
  -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f
};

void Renderer::genGlResources() {
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &APP_VBO);
  glGenBuffers(1, &INSTANCE);
  glGenBuffers(1, &BLOCK_INTS);
  glGenBuffers(1, &APP_INSTANCE);

  glGenBuffers(1, &LINE_VBO);
  glGenBuffers(1, &LINE_INSTANCE);


  glGenVertexArrays(1, &VAO);
  glGenVertexArrays(1, &APP_VAO);
  glGenVertexArrays(1, &LINE_VAO);
}

void Renderer::bindGlResourcesForInit() {
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
}

void Renderer::setupVertexAttributePointers() {
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

   // instance coord attribute
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);

  // instance texture attribute
  glBindBuffer(GL_ARRAY_BUFFER, BLOCK_INTS);
  glVertexAttribIPointer(3, 1, GL_INT, 2*sizeof(int), (void *)0);
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);

  glVertexAttribIPointer(4, 1, GL_INT, 2*sizeof(int), (void *)sizeof(int));
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1);

  glBindVertexArray(APP_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  // instance coord attribute
  glBindBuffer(GL_ARRAY_BUFFER, APP_INSTANCE);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
                        (3 * sizeof(float)) + (1 * sizeof(int)), (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);

  // instance app number attribute
  glBindBuffer(GL_ARRAY_BUFFER, APP_INSTANCE);
  glVertexAttribIPointer(3, 1, GL_INT, (3 * sizeof(float)) + (1 * sizeof(int)),
                         (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);

  // line
  glBindVertexArray(LINE_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
}

int MAX_CUBES = 1000000;

void Renderer::fillBuffers() {
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, APP_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(appVertices), appVertices,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3)) * MAX_CUBES,
               (void *)0, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, BLOCK_INTS);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(int)*2) * MAX_CUBES, (void *)0,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, APP_INSTANCE);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) + sizeof(int)) * 20,
               (void *)0, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_VBO);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void *)0,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, LINE_INSTANCE);
  glBufferData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) * 200000), (void *)0,
               GL_STATIC_DRAW);
}

Renderer::Renderer(Camera *camera, World *world) {
  this->camera = camera;
  this->world = world;

  logger = make_shared<spdlog::logger>("Renderer", fileSink);
  logger->set_level(spdlog::level::debug);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  genGlResources();
  bindGlResourcesForInit();
  fillBuffers();
  setupVertexAttributePointers();

  std::vector<std::string> images = {
      "images/bAndGrey.png",         "images/purpleRoad.png",
      "images/bAndGreySpeckled.png", "images/grass.png",
      "images/pillar.png",           "images/reactor_texture.png"};
  textures.insert(std::pair<string, Texture *>(
      "allBlocks", new Texture(images, GL_TEXTURE0)));
  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shader->use(); // may need to move into loop to use changing uniforms

  shader->setInt("allBlocks", 0);
  shader->setInt("totalBlockTypes", images.size());

  initAppTextures();

  shader->setBool("lookedAtValid", false);

  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
  glClearColor(178.0 / 256, 178.0 / 256, 178.0 / 256, 1.0f);
  glLineWidth(10.0);

  view = view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
  projection =
      glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.1f, 100.0f);
  model = glm::scale(glm::mat4(1.0f), glm::vec3(world->CUBE_SIZE));
  appModel = glm::mat4(1.0f);
}

void Renderer::initAppTextures() {
  for (int index = 0; index < 7; index++) {
    int textureN = 31 - index;
    int textureUnit = GL_TEXTURE0 + textureN;
    string textureName = "app" + to_string(index);
    textures.insert(
        std::pair<string, Texture *>(textureName, new Texture(textureUnit)));
    shader->setInt(textureName, textureN);
  }
}

void Renderer::updateTransformMatrices() {
  unsigned int modelLoc = glGetUniformLocation(shader->ID, "model");
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

  unsigned int appModelLoc = glGetUniformLocation(shader->ID, "appModel");
  glUniformMatrix4fv(appModelLoc, 1, GL_FALSE, glm::value_ptr(appModel));
  unsigned int viewLoc = glGetUniformLocation(shader->ID, "view");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  unsigned int projectionLoc = glGetUniformLocation(shader->ID, "projection");
  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void Renderer::updateCubeBuffer(CubeBuffer cubeBuffer) {
  if(cubeBuffer.damageSize > 0) {
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
    glBufferSubData(GL_ARRAY_BUFFER,
                    sizeof(glm::vec3) * cubeBuffer.damageIndex,
                    sizeof(glm::vec3) * cubeBuffer.damageSize,
                    cubeBuffer.vecs);
    glBindBuffer(GL_ARRAY_BUFFER, BLOCK_INTS);
    glBufferSubData(GL_ARRAY_BUFFER,
                    (2 * sizeof(int)) * cubeBuffer.damageIndex,
                    sizeof(int)*2*cubeBuffer.damageSize,
                    cubeBuffer.ints);
  }
}

void Renderer::addAppCube(int index, glm::vec3 pos) {
  glBindBuffer(GL_ARRAY_BUFFER, APP_INSTANCE);
  glBufferSubData(GL_ARRAY_BUFFER, (sizeof(glm::vec3) + sizeof(int)) * index,
                  sizeof(glm::vec3), &pos);
  glBufferSubData(GL_ARRAY_BUFFER,
                  sizeof(glm::vec3) * (index + 1) + sizeof(int) * index,
                  sizeof(int), &index);
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

void Renderer::drawAppDirect(X11App *app) {
  int index = world->getIndexOfApp(app);
  int screenWidth = 1920;
  int screenHeight = 1080;
  int appWidth = app->width;
  int appHeight = app->height;
  if (index >= 0) {
    glBlitNamedFramebuffer(frameBuffers[index], 0,
                           // src x1, src y1 (flip)
                           0, appHeight,
                           // end x2, end y2 (flip)
                           appWidth, 0,

                           // dest x1,y1,x2,y2
                           (screenWidth - appWidth) / 2,
                           (screenHeight - appHeight) / 2,
                           appWidth + (screenWidth - appWidth) / 2,
                           appHeight + (screenHeight - appHeight) / 2,

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

void Renderer::handleLookedAtCube() {
  Position lookedAt = world->getLookedAtCube();
  if (lookedAt.valid) {
    shader->setBool("lookedAtValid", true);
    glm::vec3 lookedAtVec = glm::vec3(lookedAt.x, lookedAt.y, lookedAt.z);
    unsigned int lookedAtLoc = glGetUniformLocation(shader->ID, "lookedAt");
    glUniform3fv(lookedAtLoc, 1, glm::value_ptr(lookedAtVec));
  } else {
    shader->setBool("lookedAtValid", false);
  }
}

void Renderer::updateShaderUniforms() {
  shader->setFloat("time", glfwGetTime());
  shader->setBool("isApp", false);
  shader->setBool("isLine", false);

  updateTransformMatrices();
  handleLookedAtCube();
}

void Renderer::renderCubes() {
  CubeBuffer updatedCubes = Cube::render();
  updateCubeBuffer(updatedCubes);
  glBindVertexArray(VAO);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 36, updatedCubes.totalSize);
}

void Renderer::renderApps() {
  X11App *app = world->getLookedAtApp();
  if (app != NULL) {
    shader->setBool("appSelected", app->isFocused());
  } else {
    shader->setBool("appSelected", false);
  }

  shader->setBool("isApp", true);
  glBindVertexArray(APP_VAO);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, world->getAppCubes().size());

  if (app != NULL && app->isFocused()) {
    drawAppDirect(app);
  }
  shader->setBool("isApp", false);
}

void Renderer::renderLines() {
  shader->setBool("isLine", true);
  glBindVertexArray(LINE_VAO);
  glDrawArrays(GL_LINES, 0, world->getLines().size()*2);
  shader->setBool("isLine", false);
}

void Renderer::render() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  view = camera->tick();
  updateShaderUniforms();
  renderCubes();
  renderApps();
  renderLines();
}

Camera *Renderer::getCamera() { return camera; }

void Renderer::registerApp(X11App *app, int index) {
  int textureN = 31 - index;
  int textureUnit = GL_TEXTURE0 + textureN;
  int textureId = textures["app" + to_string(index)]->ID;
  app->attachTexture(textureUnit, textureId);
  app->appTexture();

  unsigned int framebufferId;
  glGenFramebuffers(1, &framebufferId);
  frameBuffers.push_back(framebufferId);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferId);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, textures["app" + to_string(index)]->ID,
                         0);
}

void Renderer::deregisterApp(int index) {
  glDeleteFramebuffers(1, &frameBuffers[index]);
  frameBuffers.erase(frameBuffers.begin() + index);
}

void Renderer::toggleMeshing() {
}

Renderer::~Renderer() {
  delete shader;
  for (auto &t : textures) {
    delete t.second;
  }
}
