#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
  glm::vec3 up;
  glm::vec3 position;
  glm::vec3 front;
  bool firstMouse;
  float lastX;
  float lastY;
  float yaw;
  float pitch;
 public:
  Camera();
  void handleTranslateForce(bool up, bool down, bool left, bool right);
  void handleRotateForce(GLFWwindow* window, double xpos, double ypos);
  ~Camera();
  glm::mat4 getViewMatrix();
};


#endif
