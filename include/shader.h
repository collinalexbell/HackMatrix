#ifndef __SHADER_H__
#define __SHADER_H__
#include <optional>
#include <string>
#include <unordered_map>
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class Shader
{
  // shader ids
  unsigned int vertex, fragment, geometry;
  void linkShaderProgram();
  bool createAndCompileShader(GLenum shaderType, std::string sourceCode);
  bool createShaders();
  void deleteShaders();
  void createShaderProgram();
  void loadCode(std::string,
                std::string,
                std::optional<std::string> = std::nullopt);
  void cacheLastTimeSourceCodeChangedOnDisk();
  bool sourceCodeChanged();
  void restoreUniforms() const;

  std::unordered_map<std::string, bool> boolUniforms;
  std::unordered_map<std::string, int> intUniforms;
  std::unordered_map<std::string, float> floatUniforms;
  std::unordered_map<std::string, glm::vec3> vec3Uniforms;
  std::unordered_map<std::string, glm::mat4> mat4Uniforms;
  std::unordered_map<std::string, glm::mat3> mat3Uniforms;

  std::string vertexCode;
  std::string fragmentCode;
  std::optional<std::string> geometryCode;

  fs::file_time_type lastVertexChange;
  fs::file_time_type lastFragmentChange;
  fs::file_time_type lastGeometryChange;

  std::string vertexPath;
  std::string fragmentPath;
  std::optional<std::string> geometryPath;

public:
  // the program ID
  unsigned int ID = 0;
  // constructor reads and builds the shader
  Shader(std::string vertexPath, std::string fragmentPath);
  Shader(std::string vertexPath,
         std::string geometryShader,
         std::string fragmentPath);
  ~Shader();
  void reloadIfChanged();
  // use/activate the shader
  void use();
  // utility uniform functions
  void setBool(const std::string& name, bool value);
  void setInt(const std::string& name, int value);
  void setFloat(const std::string& name, float value);
  void setVec3(const std::string& name, const glm::vec3& value);
  void setMatrix4(const std::string& name, const glm::mat4& value);
  void setMatrix3(const std::string& name, const glm::mat3& value);

};
#endif
