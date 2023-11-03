#include "cube.h"
#include <limits>
#include <memory>
#include <sstream>

int Cube::createDamageIndex = 0;
vector<glm::vec3> Cube::vecs;
vector<int> Cube::ints;
vector<int> Cube::referenceCount;
vector<int> Cube::toErase;
vector<shared_ptr<int>> Cube::indices;
shared_ptr<spdlog::logger> Cube::logger;
bool Cube::isInit = false;

void Cube::initClass() {
  isInit = true;
  logger = make_shared<spdlog::logger>("Cube", fileSink);
  logger->set_level(spdlog::level::critical);
}

glm::vec3 Cube::zeroVec(0, 0, 0);
int Cube::zeroBlock(-1);
int Cube::zeroSelected(0);

bool Cube::operator==(const Cube &cmp) {
  bool xEq = this->position().x == cmp.position().x;
  bool yEq = this->position().x == cmp.position().y;
  bool zEq = this->position().z == cmp.position().z;
  bool blockTypeEq = this->blockType() == cmp.blockType();
  return xEq && yEq && zEq && blockTypeEq;
}

Cube::Cube(const Cube &cpy) {
  if(!isInit) {
    initClass();
  }
  index = cpy.index;
}

Cube &Cube::operator=(const Cube &other) {
  if (this != &other) {
    this->index = other.index;
  }
  return *this;
}

void Cube::init(glm::vec3 position, int blockType, int selected) {
  if (!isInit) {
    initClass();
  }
  if(blockType >= 0) {
    index = make_shared<int>(vecs.size());
    logger->debug("add cube" + to_string(*index));
    logger->flush();
    if(*index < createDamageIndex) {
      createDamageIndex = *index;
    }
    referenceCount.push_back(1);
    indices.push_back(index);
    vecs.push_back(position);
    ints.push_back(blockType);
    ints.push_back(selected);
  } else {
    index = make_shared<int>(-1);
  }
}

Cube::Cube(){
  index = make_shared<int>(-1);
}

Cube::Cube(glm::vec3 position, int blockType) {
  init(position, blockType, 0);
}

Cube::Cube(glm::vec3 position, int blockType, int selected) {
  init(position, blockType, selected);
}
Cube::~Cube() {}

void Cube::remove() {
  logger->debug("remove cube" + to_string(*index));
  logger->flush();
  toErase.push_back(*index);
}

glm::vec3 Cube::position() const {
  if(*index == -1) {
    return zeroVec;
  }
  return vecs[*index];
}

glm::vec3 &Cube::position() {
  if(*index == -1) {
    return zeroVec;
  }
  return vecs[*index];
}

int Cube::blockType() const {
  if(*index == -1) {
    return zeroBlock;
  }
  return ints[*index*2];
}

int &Cube::blockType() {
  if(*index == -1) {
    return zeroBlock;
  }
  return ints[*index*2];
}

int Cube::selected() const {
  if(*index == -1) {
    return zeroSelected;
  }
  return ints[*index*2+1];
}

int &Cube::selected() {
  if(*index == -1) {
    return zeroSelected;
  }
  return ints[*index*2+1];
}


CubeBuffer Cube::render() {
  int deleteDamageIndex = vecs.size();
  int damageIndex = createDamageIndex;
  if(toErase.size()>0) {
    sort(toErase.begin(), toErase.end());
    deleteDamageIndex = toErase[0];
    damageIndex = min(deleteDamageIndex, damageIndex);

    int decrement = 0;
    for(int index = deleteDamageIndex; index < indices.size(); index++) {
      if(decrement < toErase.size() && toErase[decrement] == index) {
        decrement++;
      } else {
        logger->info("was:" + to_string(*(indices[index])));
        *(indices[index]) = *(indices[index]) - decrement;
        logger->info("is:" + to_string(*indices[index]));
      }
    }

    sort(toErase.begin(), toErase.end(), std::greater<>());
    for(auto index = toErase.begin(); index != toErase.end(); index++) {
      logger->debug("erasing:" + to_string(*index));
      logger->flush();
      vecs.erase(vecs.begin() + *index);

      ints.erase(ints.begin() + (2 * (*index)) + 1);
      ints.erase(ints.begin() + (2 * (*index)));

      indices.erase(indices.begin() + *index);
    }

    toErase.clear();

  }

  // reset createDamageIndex
  int nCubes = vecs.size();
  createDamageIndex = nCubes;

  int size = nCubes - damageIndex;
  if(size > 0) {
    return CubeBuffer{
      &vecs[damageIndex],
      &ints[damageIndex*2],
      damageIndex,
      size,
      nCubes
    };
  } else {
    return CubeBuffer{
      NULL,
      NULL,
      0,
      0,
      nCubes
    };
  }
}
