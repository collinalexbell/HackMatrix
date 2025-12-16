#pragma once

#include <array>
#include <string>
#include <glm/mat4x4.hpp>

// Common surface interface for windows we can texture (X11, Wayland, etc.)
class AppSurface {
public:
  virtual ~AppSurface() = default;

  virtual void attachTexture(int textureUnit, int textureId, size_t appIndex) = 0;
  virtual void appTexture() = 0;

  virtual void focus(unsigned long matrixWindow) = 0;
  virtual void takeInputFocus() = 0;
  virtual void unfocus(unsigned long matrixWindow) = 0;
  virtual bool isFocused() = 0;

  virtual void select() = 0;
  virtual void deselect() = 0;
  virtual bool isSelected() = 0;

  virtual void resize(int width, int height) = 0;
  virtual void resizeMove(int width, int height, int x, int y) = 0;

  virtual int getWidth() const = 0;
  virtual int getHeight() const = 0;
  virtual std::array<int, 2> getPosition() const = 0;
  virtual glm::mat4 getHeightScalar() const = 0;
  virtual int getTextureId() const = 0;
  virtual int getTextureUnit() const = 0;

  virtual std::string getWindowName() = 0;
  virtual int getPID() = 0;
  virtual size_t getAppIndex() const = 0;
};
