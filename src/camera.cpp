#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <iostream>

Camera::Camera() {
  position = glm::vec3(0.0f, 1.5f, 1.8f);
  front = glm::vec3(0.0f, 0.0f, -1.0f);
  up = glm::vec3(0.0f, 1.0f, 0.0f);
  firstMouse = true;
  yaw   = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
  pitch =  0.0f;
  lastX =  800.0f / 2.0;
  lastY =  600.0 / 2.0;

}

Camera::~Camera() {

}

void Camera::handleTranslateForce(bool up, bool down, bool left, bool right) {
  const float cameraSpeed = 0.03f; // adjust accordingly
  if (up)
    position += cameraSpeed * front;
  if (down)
    position -= cameraSpeed * front;
  if (left)
    position -= glm::normalize(glm::cross(front, this->up)) *
      cameraSpeed;
  if (right)
    position += glm::normalize(glm::cross(front, this->up)) *
      cameraSpeed;
}

void Camera::handleRotateForce(GLFWwindow* window, double xoffset, double yoffset) {

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
  this->front = glm::normalize(front);
}

glm::mat4 Camera::tick() {
  if(movements.size() > 0){
    Movement& currentMovement = movements.front();
    interpolateMovement(currentMovement);
    if(*currentMovement.isDone) {
      movements.pop();
    }
  }
  return getViewMatrix();
}

void Camera::interpolateMovement(Movement& movement) {
  double time = glfwGetTime();
  float completionRatio = (time - movement.startTime)/(movement.endTime-movement.startTime);
  if(completionRatio >= 1.0) {
    completionRatio = 1.0;
    *movement.isDone = true;
    position = movement.finishPosition;
    front = movement.finishFront;
  }
  position = movement.startPosition +
    ((movement.finishPosition-movement.startPosition) * completionRatio);

  front = movement.startFront +
    ((movement.finishFront-movement.startFront) * completionRatio);
}

glm::mat4 Camera::getViewMatrix() {
  return glm::lookAt(position, position + front, up);
}

std::shared_ptr<bool> Camera::moveTo(glm::vec3 targetPosition, glm::vec3 targetFront,
                    float moveSeconds) {
  pitch = glm::degrees(asin(targetFront.y));
  yaw = glm::degrees(atan2(targetFront.z, targetFront.x));
  std::shared_ptr<bool> isDone(new bool(false));
  double curTime = glfwGetTime();
  double finishTime = curTime + moveSeconds;

  Movement movement{
    position, targetPosition,
    front, targetFront,
    curTime, finishTime,
    isDone
  };
  movements.push(movement);
  return isDone;
}


bool Camera::isMoving() {
  return movements.size() > 0;
}
