#include "shader.h"
#include <glad/glad.h> // include glad to get the required OpenGL headers
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

#include <glm/gtc/type_ptr.hpp>

std::string retrieveShaderCode(std::string path) {
  std::ifstream shaderFile;
  // ensure ifstream objects can throw exceptions:
  shaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
  try
    {
      shaderFile.open(path.c_str());
      std::stringstream shaderStream; shaderStream << shaderFile.rdbuf();
      std::string rv = shaderStream.str();
      shaderFile.close();
      return rv;
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

void Shader::createAndCompileShader(GLenum shaderType, const std::string sourceCode) {
  unsigned int shaderId = glCreateShader(shaderType);
  const std::string::size_type size = sourceCode.size();
  char *src = new char[size + 1];   //we need extra char for NUL
  memcpy(src, sourceCode.c_str(), size + 1);
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
  if(shaderType == GL_GEOMETRY_SHADER) {
    geometry=shaderId;
    shaderName = "GEOMETRY";
  }
  printCompileErrorsIfAny(shaderId, shaderName);
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

void Shader::linkShaderProgram() {
  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  if(geometryCode.has_value()) {
    glAttachShader(ID, geometry);
  }
  glLinkProgram(ID);
  printLinkingErrors(ID);
}

void Shader::createShaders() {
  createAndCompileShader(GL_VERTEX_SHADER, vertexCode);
  createAndCompileShader(GL_FRAGMENT_SHADER, fragmentCode);
  if(geometryCode.has_value()) {
    std::cout << "creating geometry shader" << std::endl;
    createAndCompileShader(GL_GEOMETRY_SHADER, geometryCode.value());
  }
}

void Shader::deleteShaders() {
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  if(geometryCode.has_value()) {
    glDeleteShader(geometry);
  }
}

void Shader::loadCode(std::string vertexPath,
    std::string fragmentPath,
    std::optional<std::string> geometryPath) {
  vertexCode = retrieveShaderCode(vertexPath);
  fragmentCode = retrieveShaderCode(fragmentPath);
  if(geometryPath.has_value()) {
    geometryCode = retrieveShaderCode(geometryPath.value());
  }
  if(vertexCode == "" || fragmentCode == "" ||
      (geometryPath.has_value() && geometryCode.value() == "")) {
    std::cout << "ERROR::SHADER::FAILED_TO_INITIALIZE" << std::endl;
    return;
  }
}

Shader::Shader(std::string vertexPath, std::string fragmentPath) {
  loadCode(vertexPath, fragmentPath);
  createShaderProgram();
}

Shader::Shader(std::string vertexPath,
    std::string geometryPath,
    std::string fragmentPath) {
  loadCode(vertexPath, fragmentPath, geometryPath);
  createShaderProgram();
}

void Shader::createShaderProgram() {
  createShaders();
  linkShaderProgram();
  deleteShaders();
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

void Shader::setMatrix4(const std::string &name, const glm::mat4 &value) const {
  glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &value[0][0]);
}

void Shader::setMatrix3(const std::string &name, const glm::mat3 &value) const {
  glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE,
                     &value[0][0]);
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const {
  glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}
