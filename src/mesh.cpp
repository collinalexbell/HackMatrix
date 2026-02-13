#include "mesh.h"
#include <vector>

using namespace std;

static unsigned int
getFallbackWhiteTexture()
{
  static unsigned int fallbackTexture = 0;
  if (fallbackTexture != 0) {
    return fallbackTexture;
  }

  glGenTextures(1, &fallbackTexture);
  glBindTexture(GL_TEXTURE_2D, fallbackTexture);

  const unsigned char whitePixel[4] = { 255, 255, 255, 255 };
  glTexImage2D(
    GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  return fallbackTexture;
}

Mesh::Mesh(vector<Vertex> vertices,
           vector<unsigned int> indices,
           vector<MeshTexture> textures)
{
  this->vertices = vertices;
  this->indices = indices;
  this->textures = textures;

  setupMesh();
}

void
Mesh::setupMesh()
{
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               &vertices[0],
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices.size() * sizeof(unsigned int),
               &indices[0],
               GL_STATIC_DRAW);

  // vertex positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

  // vertex texture coords
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        (void*)offsetof(Vertex, TexCoords));

  // vertex normals
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
    2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

  glBindVertexArray(0);
}

void
Mesh::Draw(Shader& shader)
{

  unsigned int diffuseNr = 1;
  unsigned int specularNr = 1;
  bool hasDiffuse = false;
  for (unsigned int i = 0; i < textures.size(); i++) {
    glActiveTexture(GL_TEXTURE1 +
                    i); // activate proper texture unit before binding
    // retrieve texture number (the N in diffuse_textureN)
    string number;
    string name = textures[i].type;
    if (name == "texture_diffuse") {
      number = std::to_string(diffuseNr++);
      hasDiffuse = true;
    } else if (name == "texture_specular")
      number = std::to_string(specularNr++);

    shader.setInt((name + number).c_str(), i + 1);
    glBindTexture(GL_TEXTURE_2D, textures[i].id);
  }

  if (!hasDiffuse) {
    glActiveTexture(GL_TEXTURE1);
    shader.setInt("texture_diffuse1", 1);
    glBindTexture(GL_TEXTURE_2D, getFallbackWhiteTexture());
  }
  glActiveTexture(GL_TEXTURE0);

  // draw mesh
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
