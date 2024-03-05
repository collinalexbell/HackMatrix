#include "systems/ApplyRotation.h"
#include "components/RotateMovement.h"
#include "model.h"
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
  }
  lastRotated = curTime;
}
