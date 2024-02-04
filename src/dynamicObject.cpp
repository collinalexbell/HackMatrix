#include "dynamicObject.h"

DynamicCube::DynamicCube(glm::vec3 position, glm::vec3 size)
  : position(position), size(size) {};

Renderable DynamicCube::makeRenderable() {
    Renderable renderable;

    // Half dimensions
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // Vertex positions relative to the center
    glm::vec3 vertices[] = {
      {hx, hy, hz}, {-hx, hy, hz}, {-hx, -hy, hz}, {hx, -hy, hz}, // Front face
      {hx, hy, -hz}, {-hx, hy, -hz}, {-hx, -hy, -hz}, {hx, -hy, -hz}, // Back face
      {hx, hy, hz}, {hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, // Right face
      {-hx, hy, hz}, {-hx, -hy, hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, // Left face
      {hx, hy, hz}, {hx, hy, -hz}, {-hx, hy, -hz}, {-hx, hy, hz}, // Top face
      {hx, -hy, hz}, {hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, -hy, hz}  // Bottom face
    };

    // Triangles (two per face)
    int indices[] = {
      0, 1, 2, 0, 2, 3, // Front
      4, 7, 6, 4, 6, 5, // Back
      8, 11, 10, 8, 10, 9, // Right
      12, 13, 14, 12, 14, 15, // Left
      16, 19, 18, 16, 18, 17, // Top
      20, 21, 22, 20, 22, 23  // Bottom
    };

    // Fill vertices for each triangle
    for (int i = 0; i < 36; i++) {
      renderable.vertices.push_back(position + vertices[indices[i]]);
    }

    return renderable;
  }

Renderable DynamicObjectSpace::makeRenderable() {
  _damaged = false;
  Renderable combinedRenderable;

  // Iterate over all dynamic objects and combine their renderables
  for (const auto &obj : objects) {
    Renderable objRenderable = obj->makeRenderable();
    combinedRenderable.vertices.insert(combinedRenderable.vertices.end(),
                                       objRenderable.vertices.begin(),
                                       objRenderable.vertices.end());
  }

  return combinedRenderable;
}

void DynamicObjectSpace::addObject(shared_ptr<DynamicObject> obj) {
  objects.push_back(obj);
}

bool DynamicObjectSpace::damaged() {
  return _damaged;
}
