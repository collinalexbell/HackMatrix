#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "shader.h"

using namespace std;

struct Vertex {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
};

struct MeshTexture {
  unsigned int id;
  string type;
};

class Mesh {
public:
  // mesh data
  vector<Vertex> vertices;
  vector<unsigned int> indices;
  vector<MeshTexture> textures;

  Mesh(vector<Vertex> vertices, vector<unsigned int> indices,
       vector<MeshTexture> textures);
  void Draw(Shader &shader);

private:
  //  render data
  unsigned int VAO, VBO, EBO;

  void setupMesh();
};
