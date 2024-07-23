#include "systems/ApplyTranslation.h"
#include "components/TranslateMovement.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/transform.hpp"
#include "model.h"
#include "components/Parent.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>

double MIN_DELTA = 0.0001;
void
systems::applyTranslations(std::shared_ptr<EntityRegistry> registry)
{
  // initialize to current time on first call
  static double lastTranslated = glfwGetTime();

  double curTime = glfwGetTime();

  auto toRotate = registry->view<Positionable, TranslateMovement>();
  for (auto [entity, positionable, translateMovement] : toRotate.each()) {

    glm::vec3 direction = glm::normalize(translateMovement.delta);
    float distance =
      translateMovement.unitsPerSecond * (curTime - lastTranslated);
    glm::vec3 delta = direction * distance;

    // Ensure degrees are always positive for 'min' calculation
    delta = glm::abs(delta);
    delta = min(delta, glm::abs(translateMovement.delta));

    // Add the signs back in (after taking min)
    delta *= glm::sign(translateMovement.delta);

    translateMovement.delta -= delta;

    positionable.pos += delta;

    if (glm::length(translateMovement.delta) <
        MIN_DELTA) { // Account for negatives
      if (translateMovement.onFinish.has_value()) {
        (*translateMovement.onFinish)();
      }
      registry->remove<TranslateMovement>(entity);
    }
    positionable.damage();

    auto parent = registry->try_get<Parent>(entity);
    if (parent != NULL) {
      for (auto childId : parent->childrenIds) {
        auto childEntityOpt = registry->locateEntity(childId);
        if (childEntityOpt.has_value()) {
          auto childEntity = childEntityOpt.value();
          auto& childPositionable = registry->get<Positionable>(childEntity);
          childPositionable.pos += delta;
          childPositionable.damage();
        }
      }
    }
  }
  lastTranslated = curTime;
}
