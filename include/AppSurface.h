#pragma once

#include <array>
#include <string>
#include <glm/mat4x4.hpp>

// Common interface for Wayland surfaces rendered in the world.
class AppSurface {
public:
  virtual ~AppSurface() = default;

  virtual int createTexture() = 0;
  virtual void appTexture() = 0;

  virtual void focus() = 0;
  virtual void takeInputFocus() = 0;
  virtual void unfocus() = 0;
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

  virtual std::string getWindowName() = 0;
  virtual int getPID() = 0;
};
