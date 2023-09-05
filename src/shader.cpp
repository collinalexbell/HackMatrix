#include "shader.h"
#include <glad/glad.h> // include glad to get the required OpenGL headers
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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

void printCompileErrorsIfAny(unsigned int shaderId, std::string shaderName) {
  int success;
  char infoLog[512];
  glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(shaderId, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::" << shaderName << "::COMPILATION_FAILED\n" <<
      infoLog << std::endl;
  };
}

unsigned int createAndCompileShader(GLenum shaderType, std::string sourceCode) {
  unsigned int rv = glCreateShader(shaderType);
  const char* src =sourceCode.c_str();
  glShaderSource(rv, 1, &src, NULL);
  glCompileShader(rv);

  std::string shaderName = "UNKNOWN";
  if(shaderType == GL_FRAGMENT_SHADER) {
    shaderName = "FRAGMENT";
  }
  if(shaderType == GL_VERTEX_SHADER) {
    shaderName = "VERTEX";
  }
  printCompileErrorsIfAny(rv, shaderName);
  return rv;
}

unsigned int linkShaderProgram(unsigned int vertex, unsigned int fragment) {
  unsigned int ID;
  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);
  return ID;
}

void printLinkingErrors(unsigned int shaderId){
  int success;
  char infoLog[512];
  glGetProgramiv(shaderId, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(shaderId, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" <<
      infoLog << std::endl;
  }
}

Shader::Shader(const char* vertexPath, const char* fragmentPath) {

  std::string vertexCode = retrieveShaderCode(vertexPath);
  std::string fragmentCode = retrieveShaderCode(fragmentPath);
  if(vertexCode == "" || fragmentCode == "") {
    std::cout << "ERROR::SHADER::FAILED_TO_INITIALIZE" << std::endl;
    return;
  }

  unsigned int vertex, fragment;
  vertex = createAndCompileShader(GL_VERTEX_SHADER, vertexCode);
  fragment = createAndCompileShader(GL_FRAGMENT_SHADER, fragmentCode);

  ID = linkShaderProgram(vertex, fragment);
  printLinkingErrors(ID);

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
