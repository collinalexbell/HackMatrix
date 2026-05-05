#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <xkbcommon/xkbcommon.h>

class Shader;

class TypedKeyOverlay
{
public:
  TypedKeyOverlay() = default;
  ~TypedKeyOverlay();

  void recordKeysym(xkb_keysym_t sym);
  void render(Shader* shader,
              GLuint vao,
              GLuint vbo,
              float screenWidth,
              float screenHeight,
              bool appFocused);

private:
  mutable std::mutex mutex;
  std::vector<std::string> tokens;
  double expiresAt = 0.0;
  GLuint texture = 0;
};
