#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

Camera::Camera() {
  cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
  cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
  cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
  firstMouse = true;
  yaw   = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
  pitch =  0.0f;
  lastX =  800.0f / 2.0;
  lastY =  600.0 / 2.0;

}

Camera::~Camera() {

}

void Camera::handleTranslateForce(bool up, bool down, bool left, bool right) {
  const float cameraSpeed = 0.35f; // adjust accordingly
  if (up)
    cameraPos += cameraSpeed * cameraFront;
  if (down)
    cameraPos -= cameraSpeed * cameraFront;
  if (left)
    cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) *
      cameraSpeed;
  if (right)
    cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) *
      cameraSpeed;
}

void Camera::handleRotateForce(GLFWwindow* window, double xpos, double ypos) {
  if (firstMouse)
    {
      lastX = xpos;
      lastY = ypos;
      firstMouse = false;
    }
  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos;
  lastX = xpos;
  lastY = ypos;
  float sensitivity = 0.1f; xoffset *= sensitivity;
  yoffset *= sensitivity;
  yaw += xoffset;
  pitch += yoffset;
  if(pitch > 89.0f)
    pitch = 89.0f;
  if(pitch < -89.0f)
    pitch = -89.0f;
  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  cameraFront = glm::normalize(front);
}

glm::mat4 Camera::getViewMatrix() {
  return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}
