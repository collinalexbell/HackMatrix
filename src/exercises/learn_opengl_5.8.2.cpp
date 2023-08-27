// checkout 5da6932 to compile this exercise
#include <iostream>
#include "renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

int count = 0;

float vertices[][9] = {{
    // first triangle
    0.5f, 0.5f, 0.0f, // top right
    0.5f, -0.5f, 0.0f, // bottom right
    -0.5f, 0.5f, 0.0f // top left
  },{
    // second triangle
    0.5f, -0.5f, 0.0f, // bottom right
    -0.5f, -0.5f, 0.0f, // bottom left
    -0.5f, 0.5f, 0.0f // top left
  }};

// use vbo[] and vao[] so I don't have to change the Renderer header
unsigned int vbo[2];
unsigned int vao[2];

unsigned int indices[] = { // note that we start from 0!
  0, 1, 3, // first triangle
  1, 2, 3 // second triangle
};

const char *vertexShaderSource = "#version 330 core\n"
  "layout (location = 0) in vec3 aPos;\n"
  "void main()\n"
  "{\n"
  " gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
  "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
  "out vec4 FragColor;\n"
  "void main()\n"
  "{\n"
  "FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
  "}\0";

void Renderer::initFragmentShader() {
  int success;
  char infoLog[512];
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" <<
      infoLog << std::endl;
  }
}

void Renderer::initVertexShader() {
  int success;
  char infoLog[512];
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" <<
      infoLog << std::endl;
  }
}

void Renderer::buildShaderProgram() {
  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}


Renderer::Renderer() {

  glGenBuffers(1, &vbo[0]);
  glGenBuffers(1, &vbo[1]);
  glGenVertexArrays(1, &vao[0]);
  glGenVertexArrays(1, &vao[1]);

  glBindVertexArray(vao[0]);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]), vertices[0], GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

  glBindVertexArray(vao[1]);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[1]), vertices[1], GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


  initVertexShader();
  initFragmentShader();
  buildShaderProgram();

  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
}

void Renderer::render() {
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindVertexArray(vao[0]);
  glEnableVertexAttribArray(0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(vao[1]);
  glEnableVertexAttribArray(0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}
