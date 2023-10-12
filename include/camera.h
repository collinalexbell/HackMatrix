#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

class Camera {
  glm::vec3 up;
  bool firstMouse;
  float lastX;
  float lastY;
  float yaw;
  float pitch;
 public:
  glm::vec3 front;
  glm::vec3 position;
  Camera();
  void handleTranslateForce(bool up, bool down, bool left, bool right);
  void handleRotateForce(GLFWwindow* window, double xoffset, double yoffset);
  ~Camera();
  glm::mat4 getViewMatrix();
  std::shared_ptr<bool> moveTo(glm::vec3 position, glm::vec3 front, float moveSeconds);
};


#endif
