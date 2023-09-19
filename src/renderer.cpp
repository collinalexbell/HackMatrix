#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include "camera.h"
#include "app.h"
#include <vector>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
  glGenBuffers(1, &INSTANCE);
  glGenVertexArrays(1, &VAO);
}

void Renderer::bindGlResourcesForInit() {
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
}

void Renderer::setupVertexAttributePointers() {
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // instance coord attribute
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
}

void Renderer::fillBuffers() {
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 200000, (void*)0 , GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * world->getCubes().size(), world->getCubes().data());
}

Renderer::Renderer(Camera* camera, World* world) {
  this->camera = camera;
  this->world = world;
  glEnable(GL_DEPTH_TEST);
  genGlResources();
  bindGlResourcesForInit();
  fillBuffers();
  setupVertexAttributePointers();

  textures.insert(std::pair<string, Texture*>("container",
                                             new Texture(GL_TEXTURE0)));
  textures.insert(std::pair<string, Texture*>("face",
                                              new Texture("images/awesomeface.png", GL_TEXTURE1)));

  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shader->use(); // may need to move into loop to use changing uniforms
  shader->setInt("texture1", 0);
  shader->setInt("texture2", 1);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures["container"]->ID);
  appTexture();




  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  orthographicMatrix = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, 0.1f, 100.0f);


  view = glm::mat4(1.0f);
  // note that we’re translating the scene in the reverse direction
  view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

  projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f,
                                100.0f);

  // this can be used to rotate an entire chunk (call to glDrawArraysInstanced)
  model = glm::mat4(1.0f);
  //model = glm::rotate(model, glm::radians(-55.0f),
  //                   glm::vec3(1.0f, 0.0f, 0.0f));
  //model = glm::scale(model, glm::vec3(2,2,1));

}

void Renderer::computeTransform() {
  model = glm::rotate(model, glm::radians(3.0f), glm::vec3(0.0, 0.0, 1));
}

void Renderer::updateTransformMatrices() {
  unsigned int modelLoc = glGetUniformLocation(shader->ID,"model");
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

  unsigned int viewLoc = glGetUniformLocation(shader->ID,"view");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  unsigned int projectionLoc = glGetUniformLocation(shader->ID,"projection");
  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

/*
glm::vec3 cubePositions[] = {
  glm::vec3( 0.0f, 0.0f, 0.0f),
  glm::vec3( 2.0f, 5.0f, -15.0f),
  glm::vec3( 15.0f, 15.0f, -15.0f),
  glm::vec3(-1.5f, -2.2f, -2.5f),
  glm::vec3(-3.8f, -2.0f, -12.3f),
  glm::vec3( 2.4f, -0.4f, -3.5f),
  glm::vec3(-1.7f, 3.0f, -7.5f),
  glm::vec3( 1.3f, -2.0f, -2.5f),
  glm::vec3( 1.5f, 2.0f, -2.5f),
  glm::vec3( 1.5f, 0.2f, -1.5f),
  glm::vec3(-1.3f, 1.0f, -1.5f)
};
*/


std::vector<glm::vec3> cubePositions = {
  glm::vec3( -2.0f, 0.0f, -5.0f),
  glm::vec3( -2.0f, 1.0f, -5.0f),
  glm::vec3( -2.0f, 2.0f, -5.0f),
  glm::vec3( -2.0f, 3.0f, -5.0f),

  glm::vec3( 2.0f, 0.0f, -5.0f),
  glm::vec3( 2.0f, 1.0f, -5.0f),
  glm::vec3( 2.0f, 2.0f, -5.0f),
  glm::vec3( 2.0f, 3.0f, -5.0f),

  glm::vec3( 1.0f, 3.0f, -5.0f),
  glm::vec3( 0.0f, 3.0f, -5.0f),
  glm::vec3( -1.0f, 3.0f, -5.0f)
};

void Renderer::addCube(int index) {
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*index, sizeof(glm::vec3), &world->getCubes()[index]);
}

float FPS_60 = 1.0/1.0;
bool ran = false;
void Renderer::render() {

  // This is a hacky test, need to put this into a seperate method or something
  if(!ran) {
        ran = true;
  }
  // This is a hacky test, need to put this into a seperate method or something

  //computeTransform();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  view = camera->getViewMatrix();
  updateTransformMatrices();
  glDrawArraysInstanced(GL_TRIANGLES, 0, 36, world->getCubes().size());
}

Camera* Renderer::getCamera() {
  return camera;
}

Renderer::~Renderer() {
  delete shader;
  for(auto& t: textures) {
    delete t.second;
  }
}
