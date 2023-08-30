#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>

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

Renderer::Renderer() {
  genGlResources();
  bindGlResourcesForInit();

  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);


  texture = new Texture("images/container.jpg");
  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");

  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
}

void Renderer::render() {
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  shader->use();
  glBindTexture(GL_TEXTURE_2D, texture->ID);
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

Renderer::~Renderer() {
  delete shader;
  delete texture;
}
