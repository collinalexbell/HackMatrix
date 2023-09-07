#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

float vertices[] = {
  // positions // colors // texture coords
  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
  0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
  -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
  -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // top left
};
unsigned int indices[] = { // note that we start from 0!
  0, 1, 3, // first triangle
  1, 2, 3 // second triangle
};

void Renderer::genGlResources() {
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  glGenVertexArrays(1, &VAO);
}

void Renderer::bindGlResourcesForInit() {
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
}

void Renderer::setupVertexAttributePointers() {
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
}

void Renderer::fillBuffers() {
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

Renderer::Renderer() {
  genGlResources();
  bindGlResourcesForInit();
  fillBuffers();
  setupVertexAttributePointers();

  textures.insert(std::pair<string, Texture*>("container",
                                              new Texture("images/container.jpg", GL_TEXTURE0)));
  textures.insert(std::pair<string, Texture*>("face",
                                              new Texture("images/awesomeface.png", GL_TEXTURE1)));

  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shader->use(); // may need to move into loop to use changing uniforms
  shader->setInt("texture1", 0);
  shader->setInt("texture2", 1);

  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  angle = 0.0;

}

void Renderer::computeTransform() {
  trans = glm::mat4(1.0f);
  trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
  trans = glm::rotate(trans, glm::radians(angle), glm::vec3(0.0, 0.0, 1.0));
  trans = glm::scale(trans, glm::vec3(0.5, 0.5, 0.5));
}

void Renderer::render() {
  angle = angle + 1;
  computeTransform();

  glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, textures["container"]->ID);
  glBindTexture(GL_TEXTURE_2D, textures["face"]->ID);

  unsigned int transformLoc = glGetUniformLocation(shader->ID,"transform");
  glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

Renderer::~Renderer() {
  delete shader;
  for(auto& t: textures) {
    delete t.second;
  }
}
