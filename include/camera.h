#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <queue>

using namespace std;

struct Movement {
  glm::vec3 startPosition;
  glm::vec3 finishPosition;
  glm::vec3 startFront;
  glm::vec3 finishFront;
  double startTime;
  double endTime;
  shared_ptr<bool> isDone;
};

class Camera {
  glm::vec3 up;
  bool firstMouse;
  bool viewUpdated = true;
  bool _projectionMatrixUpdated = true;
  float lastX;
  float lastY;
  float yaw;
  float pitch;
  queue<Movement> movements;
  void interpolateMovement(Movement& movement);
  glm::mat4 viewMatrix;
  glm::mat4 projectionMatrix;
public:
  glm::vec3 front;
  glm::vec3 position;
  Camera();
  void handleTranslateForce(bool up, bool down, bool left, bool right);
  void handleRotateForce(GLFWwindow* window, double xoffset, double yoffset);
  ~Camera();
  void tick();
  bool isMoving();
  std::shared_ptr<bool> moveTo(glm::vec3 position, glm::vec3 front, float moveSeconds);
  float getYaw() {return yaw;}
  float getPitch() {return pitch;}
  glm::mat4 &getViewMatrix();
  bool viewMatrixUpdated();
  glm::mat4 &getProjectionMatrix(bool isRenderLoop=false);
  bool projectionMatrixUpdated();
};


#endif
