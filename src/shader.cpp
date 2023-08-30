#include "shader.h"
#include <glad/glad.h> // include glad to get the required OpenGL headers
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

struct ShaderCodes {
  std::string vertex;
  std::string fragment;
};

std::string retrieveShaderCode(const char* path) {
  std::ifstream shaderFile;
  // ensure ifstream objects can throw exceptions:
  shaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
  try
    {
      shaderFile.open(path);
      std::stringstream shaderStream;
      shaderStream << shaderFile.rdbuf();
      shaderFile.close();
      return shaderStream.str();
    }
  catch(std::ifstream::failure e) {
    std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
    return "";
  }
}

void printCompileErrors(unsigned int shaderId, char* shaderName) {
  int success;
  char infoLog[512];
  glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(vertex, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::" << shaderName << "::COMPILATION_FAILED\n" <<
      infoLog << std::endl;
  };
}

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
  // 1. retrieve the vertex/fragment source code from filePath
  ShaderCodes codes;
  codes.vertex = retrieveShaderCode(vertexPath);
  codes.fragment = retrieveShaderCode(fragmentPath);
  if(codes.vertex == "" || codes.fragment == "") {
    std::cout << "ERROR::SHADER::FAILED_TO_INITIALIZE" << std::endl;
    return;
  }
  const char* vShaderCode = codes.vertex.c_str();
  const char* fShaderCode = codes.fragment.c_str();

  // 2. compile shaders
  unsigned int vertex, fragment;

  vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vShaderCode, NULL);
  glCompileShader(vertex);
  printCompileErrors(vertex, "VERTEX");

  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fShaderCode, NULL);
  glCompileShader(fragment);
  printCompileErrors(fragment, "FRAGMENT");

  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);
  // print linking errors if any
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(ID, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" <<
      infoLog << std::endl;
  }
  // delete shaders; they’re linked into our program and no longer necessary
  glDeleteShader(vertex);
  glDeleteShader(fragment);
}

void Shader::use() {
  glUseProgram(ID);
}

void Shader::setBool(const std::string &name, bool value) const
{
  glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}
void Shader::setInt(const std::string &name, int value) const
{
  glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}
void Shader::setFloat(const std::string &name, float value) const
{
  glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}
