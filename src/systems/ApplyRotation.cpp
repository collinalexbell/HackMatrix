#include "systems/ApplyRotation.h"
#include "components/RotateMovement.h"
#include "glm/gtx/transform.hpp"
#include "model.h"
#include "components/Parent.h"
#include <GLFW/glfw3.h>

double MIN_ROTATION = 0.0001;
void systems::applyRotation(std::shared_ptr<EntityRegistry> registry) {
  // initialize to current time on first call
  static double lastRotated = glfwGetTime();

  double curTime = glfwGetTime();

  auto toRotate = registry->view<Positionable, RotateMovement>();
  for (auto [entity, positionable, rotateMovement] : toRotate.each()) {
    auto degreesToRotate =
        rotateMovement.degreesPerSecond * (curTime - lastRotated);

    // Ensure degrees are always positive for 'min' calculation
    degreesToRotate = fabs(degreesToRotate);
    degreesToRotate = min(degreesToRotate, fabs(rotateMovement.degrees));

    // Apply sign of rotation
    if (rotateMovement.degrees < 0) {
      degreesToRotate *= -1.0;
    }

    rotateMovement.degrees -= degreesToRotate;

    positionable.rotate = positionable.rotate + (rotateMovement.axis * glm::vec3(degreesToRotate));

    if (fabs(rotateMovement.degrees) < MIN_ROTATION) { // Account for negatives
      if(rotateMovement.onFinish.has_value()) {
        (*rotateMovement.onFinish)();
      }
      registry->remove<RotateMovement>(entity);
    }
    positionable.damage();

    auto parent = registry->try_get<Parent>(entity);
    if (parent != NULL) {
      for (auto childId : parent->childrenIds) {
        auto childEntityOpt = registry->locateEntity(childId);
        if(childEntityOpt.has_value()) {
          auto childEntity = childEntityOpt.value();

          auto &childPositionable = registry->get<Positionable>(childEntity);

          // Calculate child's position relative to parent
          glm::vec3 relativePosition =
              childPositionable.pos - positionable.pos;

          // Rotate the relative position vector
          auto rotationMatrix = glm::rotate(glm::radians((float)degreesToRotate),
                                            rotateMovement.axis);

          glm::vec3 rotatedPosition = rotationMatrix * glm::vec4(relativePosition, 1.0f);

          // Calculate child's new world position
          childPositionable.pos =
              rotatedPosition + positionable.pos;

          childPositionable.rotate += degreesToRotate;
        }
      }
    }
  }
  lastRotated = curTime;
}
