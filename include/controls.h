#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

void mouseCallback (GLFWwindow* window, double xpos, double ypos);
void handleControls(GLFWwindow* window, Camera* camera);
void handleEscape(GLFWwindow* window);
#endif
