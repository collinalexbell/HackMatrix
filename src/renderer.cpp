#include "renderer.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>

int count = 0;

float vertices[] = {
  0.5f, 0.5f, 0.0f, // top right
  0.5f, -0.5f, 0.0f, // bottom right
  -0.5f, -0.5f, 0.0f, // bottom left
  -0.5f, 0.5f, 0.0f // top left
};
unsigned int indices[] = { // note that we start from 0!
  0, 1, 3, // first triangle
  1, 2, 3 // second triangle
};

const char *vertexShaderSource = "#version 330 core\n"
  "layout (location = 0) in vec3 aPos;\n"
  "out float isLeft;\n"
  "void main()\n"
  "{\n"
  " if(aPos.x < 0.0){\n"
  "   isLeft=1.0;\n"
  " }else{\n"
  "   isLeft=0.0;\n"
  " }\n"
  " gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
  "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
  "in float isLeft;\n"
  "uniform float red;\n"
  "out vec4 FragColor;\n"
  "void main()\n"
  "{\n"
    "  FragColor = vec4(red, isLeft, 0.2f, 1.0f);\n"
  "}\0";

void Renderer::initFragmentShader() {
  int success;
  char infoLog[512];
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" <<
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

  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
  glGenVertexArrays(1, &VAO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        (void*)0);
  glEnableVertexAttribArray(0);


  initVertexShader();
  initFragmentShader();
  buildShaderProgram();

  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
}

void Renderer::render() {
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  float timeValue = glfwGetTime();
  float redValue = sin(timeValue) / 2.0f + 0.5f;
  int vertexColorLocation = glGetUniformLocation(shaderProgram, "red");
  glUniform1f(vertexColorLocation, redValue);
  glUseProgram(shaderProgram);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
