#include "texture.h"
#include "renderer.h"
#include "shader.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

float vertices[] = {
  -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
  0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
  -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
  0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
  -0.5f, 0.5f, -0.5f, 0.0f, 1.0f
};

void Renderer::genGlResources() {
  glGenBuffers(1, &VBO);
  glGenVertexArrays(1, &VAO);
}

void Renderer::bindGlResourcesForInit() {
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
}

void Renderer::setupVertexAttributePointers() {
  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
}

void Renderer::fillBuffers() {
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
}

Renderer::Renderer() {
  glEnable(GL_DEPTH_TEST);
  genGlResources();
  bindGlResourcesForInit();
  fillBuffers();
  setupVertexAttributePointers();

  textures.insert(std::pair<string, Texture*>("container",
                                              new Texture("images/container.jpg", GL_TEXTURE0)));
  textures.insert(std::pair<string, Texture*>("face",
                                              new Texture("images/awesomeface.png", GL_TEXTURE1)));

  shader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  shader->use(); // may need to move into loop to use changing uniforms
  shader->setInt("texture1", 0);
  shader->setInt("texture2", 1);

  //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //  normal
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  angle = 0.0;
  orthographicMatrix = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, 0.1f, 100.0f);


  view = glm::mat4(1.0f);
  // note that we’re translating the scene in the reverse direction
  view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

  projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f,
                                100.0f);

  model = glm::mat4(1.0f);
  model = glm::rotate(model, glm::radians(-55.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f));
  //model = glm::scale(model, glm::vec3(2,2,1));

  cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
  cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
  cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
  cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);


  firstMouse = true;
  yaw   = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
  pitch =  0.0f;
  lastX =  800.0f / 2.0;
  lastY =  600.0 / 2.0;
}

void Renderer::mouseCallback (GLFWwindow* window, double xpos, double ypos) {
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
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
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
    std::cout << "called" << std::endl;
}

void Renderer::computeTransform() {
  model = glm::rotate(model, glm::radians(3.0f), glm::vec3(0.0, 0.0, 1));
}

void Renderer::updateTransformMatrices() {
  unsigned int modelLoc = glGetUniformLocation(shader->ID,"model");
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

  unsigned int viewLoc = glGetUniformLocation(shader->ID,"view");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

  unsigned int projectionLoc = glGetUniformLocation(shader->ID,"projection");
  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

glm::vec3 cubePositions[] = {
  glm::vec3( 0.0f, 0.0f, 0.0f),
  glm::vec3( 2.0f, 5.0f, -15.0f),
  glm::vec3(-1.5f, -2.2f, -2.5f),
  glm::vec3(-3.8f, -2.0f, -12.3f),
  glm::vec3( 2.4f, -0.4f, -3.5f),
  glm::vec3(-1.7f, 3.0f, -7.5f),
  glm::vec3( 1.3f, -2.0f, -2.5f),
  glm::vec3( 1.5f, 2.0f, -2.5f),
  glm::vec3( 1.5f, 0.2f, -1.5f),
  glm::vec3(-1.3f, 1.0f, -1.5f)
};

glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
void Renderer::moveCamera() {
  const float radius = 10.0f;
  view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}

void Renderer::handleKeys(bool up, bool down, bool left, bool right) {
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

void Renderer::render() {
  //computeTransform();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, textures["container"]->ID);
  glBindTexture(GL_TEXTURE_2D, textures["face"]->ID);
  moveCamera();
  updateTransformMatrices();
  for (unsigned int i = 0; i < 10; i++){
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, cubePositions[i]);
    float angle = 20.0f * i;
    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
    unsigned int modelLoc = glGetUniformLocation(shader->ID,"model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
}

Renderer::~Renderer() {
  delete shader;
  for(auto& t: textures) {
    delete t.second;
  }
}
