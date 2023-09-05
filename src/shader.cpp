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
      std::stringstream shaderStream; shaderStream << shaderFile.rdbuf();
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

void Shader::createAndCompileShader(GLenum shaderType, std::string sourceCode) {
  unsigned int shaderId = glCreateShader(shaderType);
  const char* src =sourceCode.c_str();
  glShaderSource(shaderId, 1, &src, NULL);
  glCompileShader(shaderId);

  std::string shaderName = "UNKNOWN";
  if(shaderType == GL_FRAGMENT_SHADER) {
    fragment=shaderId;
    shaderName = "FRAGMENT";
  }
  if(shaderType == GL_VERTEX_SHADER) {
    vertex=shaderId;
    shaderName = "VERTEX";
  }
  printCompileErrorsIfAny(shaderId, shaderName);
}

void Shader::linkShaderProgram() {
  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);
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

  createAndCompileShader(GL_VERTEX_SHADER, vertexCode);
  createAndCompileShader(GL_FRAGMENT_SHADER, fragmentCode);
  linkShaderProgram();
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
