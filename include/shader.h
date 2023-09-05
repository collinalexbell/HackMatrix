#ifndef __SHADER_H__
#define __SHADER_H__
#include <string>
#include "glad/glad.h"

class Shader
{
  // shader ids
  unsigned int vertex, fragment;
  void linkShaderProgram();
  void createAndCompileShader(GLenum shaderType, std::string sourceCode);
  void createShaders();
  void deleteShaders();
  void loadCode(std::string, std::string);
  std::string vertexCode;
  std::string fragmentCode;

 public:
  // the program ID
  unsigned int ID;
  // constructor reads and builds the shader
  Shader(std::string vertexPath, std::string fragmentPath);
  // use/activate the shader
  void use();
  // utility uniform functions
  void setBool(const std::string &name, bool value) const;
  void setInt(const std::string &name, int value) const;
  void setFloat(const std::string &name, float value) const;
};
#endif
