#ifndef __SHADER_H__
#define __SHADER_H__
#include <optional>
#include <string>
#include "glad/glad.h"
#include <glm/glm.hpp>

class Shader
{
  // shader ids
  unsigned int vertex, fragment;
  void linkShaderProgram();
  void createAndCompileShader(GLenum shaderType, std::string sourceCode);
  void createShaders();
  void deleteShaders();
  void createShaderProgram();
  void loadCode(std::string, std::string, std::optional<std::string>);
  std::string vertexCode;
  std::string fragmentCode;
  std::optional<std::string> geometryCode;


 public:
  // the program ID
  unsigned int ID;
  // constructor reads and builds the shader
  Shader(std::string vertexPath, std::string fragmentPath);
  Shader(std::string vertexPath,
      std::string geometryShader,
      std::string fragmentPath);
  // use/activate the shader
  void use();
  // utility uniform functions
  void setBool(const std::string &name, bool value) const;
  void setInt(const std::string &name, int value) const;
  void setFloat(const std::string &name, float value) const;
  void setVec3(const std::string &name, const glm::vec3 &value) const;
  void setMatrix4(const std::string &name, const glm::mat4 &value) const;
  void setMatrix3(const std::string &name, const glm::mat3 &value) const;
};
#endif
