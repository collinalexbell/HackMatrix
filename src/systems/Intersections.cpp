
#include "systems/Intersections.h"
#include "model.h"


namespace systems {
  void emplaceBoundingSphere(std::shared_ptr<EntityRegistry> registry,
                                       entt::entity entity) {
    auto [model, positionable] = registry->get<Model, Positionable>(entity);

    auto boundingSphere = model.getBoundingSphere();
    boundingSphere.center += positionable.pos;
    boundingSphere.radius *= positionable.scale;
    registry->emplace_or_replace<BoundingSphere>(entity,
                                                 boundingSphere.center,
                                                 boundingSphere.radius);
  }

  bool intersect(const BoundingSphere &sphere, glm::vec3 position,
                 glm::vec3 direction, float maxDistance) {
    // Calculate the vector from the ray origin to the sphere center
    glm::vec3 sphereToRay = position - sphere.center;

    // Calculate the squared distance from the sphere to the ray
    float a = glm::dot(direction, direction);
    float b = 2.0f * glm::dot(sphereToRay, direction);
    float c =
        glm::dot(sphereToRay, sphereToRay) - sphere.radius * sphere.radius;

    // Calculate the discriminant
    float discriminant = b * b - 4.0f * a * c;

    // If the discriminant is negative, there are no real roots, so the ray
    // misses the sphere
    if (discriminant < 0.0f)
      return false;

    // Calculate the nearest root (intersection point)
    float sqrtDiscriminant = sqrt(discriminant);
    float nearestRoot = (-b - sqrtDiscriminant) / (2.0f * a);

    // If the nearest root is within the maximum distance, the ray intersects
    // the sphere
    if (nearestRoot >= 0.0f && nearestRoot <= maxDistance)
      return true;

    // If the nearest root is behind the ray origin or beyond the maximum
    // distance, check if the farther root is within the maximum distance
    float fartherRoot = (-b + sqrtDiscriminant) / (2.0f * a);
    return fartherRoot >= 0.0f && fartherRoot <= maxDistance;
  }
}
