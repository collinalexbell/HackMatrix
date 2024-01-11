#include "blocks.h"
#include <sstream>

using namespace blocks;

TexturePack blocks::initializeBasicPack() {
  string textureDir = "/home/collin/hogwarts/textures/majestica";
  vector<Block> blocks = {
      Block{1, "stone", "stone.png"},
      Block{2, "grass", "images/grass.png", true},
      Block{3, "dirt", "dirt.png"},
      Block{17, "oak log", "oak_log.png"},
      Block{161, "acacia leaves", "images/oak.png", true},
      Block{251, "white concrete", "white_concrete.png"},
  };
  return TexturePack(textureDir, blocks);
};

TexturePack::TexturePack(string textureDir, vector<Block> blocks) : blocks(blocks), textureDir(textureDir) {
  for(int i = 0; i < blocks.size(); i++) {
    idToIndex[blocks[i].id] = i;
  }
};


int TexturePack::textureIndexFromId(int id) {
  if (auto index = idToIndex.find(id); index != idToIndex.end()) {
    return index->second;
  } else {
    return -1;
  }
}

vector<string> TexturePack::imageNames() {
  vector<string> imageNames;
  for(auto block: blocks) {
    stringstream ss;
    if(!block.localFileName) {
      ss << textureDir << "/" << block.fileName;
      imageNames.push_back(ss.str());
    } else {
      imageNames.push_back(block.fileName);
    }
  }
  return imageNames;
}
