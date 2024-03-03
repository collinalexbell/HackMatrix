#pragma once
#include "mesh.h"
#include "shader.h"
#include <assimp/mesh.h>
#include <assimp/scene.h>

unsigned int TextureFromFile(const char *path, const string &directory,
                             bool gamma = false);

struct Light {
  glm::vec3 color;
};

struct Positionable {
  Positionable(glm::vec3 pos, float scale);
  glm::vec3 pos;
  glm::mat4 modelMatrix;
  glm::mat3 normalMatrix;
};

class Model {
public:
  Model(string path);
  void Draw(Shader &shader);

private:
  // model data
  vector<Mesh> meshes;
  vector<MeshTexture> textures_loaded;
  string directory;

  void loadModel(string path);
  void processNode(aiNode *node, const aiScene *scene);
  Mesh processMesh(aiMesh *mesh, const aiScene *scene);
  vector<MeshTexture> loadMaterialTextures(aiMaterial *mat, aiTextureType type,
                                       string typeName);
};
