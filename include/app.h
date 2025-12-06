#ifndef __APP_H__
#define __APP_H__

#include <atomic>
#include "AppSurface.h"
#include <memory>
#include <string>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>
#include <glad/glad_glx.h>
#include <glm/glm.hpp>

using namespace std;

enum IdentifierType
{
  NAME,
  CLASS,
  WINDOW,
  PID
};

struct Identifier
{
  IdentifierType type;
  string strId;
  Window win;
  int pid;
};

class X11App : public AppSurface
// the monster X11 Interface App class!!
{

  Display* display;
  int screen;
  Window appWindow;
  XWindowAttributes attrs;
  GLXFBConfig* fbConfigs;
  int fbConfigCount;
  void fetchInfo(Identifier identifier);
  int textureUnit = -1;
  int textureId = -1;
  atomic_bool focused = false;
  atomic_bool selected = false;
  X11App(Display* display, int screen);
  int x = 0;
  int y = 0;
  size_t appIndex;

public:
  X11App(X11App&& other) noexcept;
  static X11App* byName(string windowName,
                        Display* display,
                        int screen,
                        int width,
                        int height);
  static X11App* byClass(string windowClass,
                         Display* display,
                         int screen,
                         int width,
                         int height);

  static X11App* byWindow(Window window,
                          Display* display,
                          int screen,
                          int width,
                          int height);

  static X11App* byPID(int pid,
                       Display* display,
                       int screen,
                       int width,
                       int height);

  static bool initAppClass(Display* display, int screen);

  int width = 0;
  int height = 0;
  glm::mat4 heightScalar = glm::mat4(1.0);
  static glm::mat4 recomputeHeightScaler(double width, double height);
  void positionNotify(int x, int y);
  void appTexture() override;
  void attachTexture(int textureUnit, int textureId, size_t appIndex) override;
  void focus(unsigned long matrix) override;
  void takeInputFocus() override;
  void unfocus(unsigned long matrix) override;
  void resize(int width, int height) override;
  void resizeMove(int width, int height, int x, int y) override;
  bool isFocused() override;
  bool isAccessory();
  int getPID() override;
  string getWindowName() override;
  Window getWindow();
  array<int, 2> getPosition() const override;
  size_t getAppIndex() const override { return appIndex; }
  int getWidth() const override { return width; }
  int getHeight() const override { return height; }
  glm::mat4 getHeightScalar() const override { return heightScalar; }
  void select() override;
  void deselect() override;
  bool isSelected() override;
  void close();
  void larger();
  void smaller();
};

#endif
