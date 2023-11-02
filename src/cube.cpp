#include "cube.h"

vector<glm::vec3> Cube::vecs;
vector<int> Cube::ints;
vector<int> Cube::referenceCount;
vector<int> Cube::toErase;
vector<int*> Cube::indices;

bool Cube::operator==(const Cube &cmp) {
  bool xEq = this->position().x == cmp.position().x;
  bool yEq = this->position().x == cmp.position().y;
  bool zEq = this->position().z == cmp.position().z;
  bool blockTypeEq = this->blockType() == cmp.blockType();
  return xEq && yEq && zEq && blockTypeEq;
}

Cube::Cube(const Cube &cpy) {
  index = cpy.index;
  referenceCount[index]++;
}

Cube &Cube::operator=(const Cube &other) {
  if (this != &other) {
    this->index = other.index;
    referenceCount[index]++;
  }
  return *this;
}

Cube::Cube(){
  index = vecs.size();
  referenceCount.push_back(1);
  indices.push_back(&index);
  vecs.push_back(glm::vec3(0, 0, 0));
  ints.push_back(-1);
  ints.push_back(0);
}

Cube::Cube(glm::vec3 position, int blockType) {
  index = vecs.size();
  referenceCount.push_back(1);
  indices.push_back(&index);
  vecs.push_back(position);
  ints.push_back(blockType);
  ints.push_back(0);
}

Cube::Cube(glm::vec3 position, int blockType, int selected) {
  index = vecs.size();
  referenceCount.push_back(1);
  indices.push_back(&index);
  vecs.push_back(position);
  ints.push_back(blockType);
  ints.push_back(selected);
}

Cube::~Cube() {
  if(--referenceCount[index] <= 0) {
    toErase.push_back(index);
  }
}

glm::vec3 Cube::position() const {
  return vecs[index];
}

glm::vec3 &Cube::position() {
  return vecs[index];
}

int Cube::blockType() const {
  return ints[index*2];
}

int &Cube::blockType() {
  return ints[index*2];
}

int Cube::selected() const {
  return ints[index*2+1];
}

int &Cube::selected() {
  return ints[index*2+1];
}

CubeBuffer Cube::render() {
  int damageIndex = vecs.size();
  sort(toErase.begin(), toErase.end());
  if(toErase.size()>0) {
    damageIndex = toErase[0];
  }

  int decrement = 0;
  for(int index = damageIndex; index < indices.size(); index++) {
    if(decrement < toErase.size() && toErase[decrement] == index) {
      decrement++;
    } else {
      (*indices[index]) = (*indices[index]) - decrement;
    }
  }

  for(auto index = toErase.begin(); index != toErase.end(); index++) {
    vecs.erase(vecs.begin() + *index);
    ints.erase(ints.begin() + (2 * *index));
    ints.erase(ints.begin() + (2 * *index) + 1);
    indices.erase(indices.begin() + *index);
    referenceCount.erase(referenceCount.begin() + *index);
  }

  toErase.clear();

  int size = vecs.size() - damageIndex;
  if(size > 0) {
    return CubeBuffer{
      &vecs[damageIndex],
      &ints[damageIndex],
      size,
    };
  } else {
    return CubeBuffer{
      NULL,
      NULL,
      0
    };
  }
}
