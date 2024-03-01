#pragma once
#include <assimp/mesh.h>
#include <string>
#include <assimp/scene.h> // For working with the model data
using namespace std;

aiMesh* loadFbx(string);
