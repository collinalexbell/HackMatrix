#include "blocks.h"
#include <sstream>

using namespace blocks;

shared_ptr<TexturePack> blocks::initializeBasicPack() {
  string textureDir = "/home/collin/hogwarts/textures/majestica";
  vector<Block> blocks = {
      Block{1, "stone", "stone.png"},
      Block{2, "grass", "images/grass.png", true},
      Block{3, "dirt", "dirt.png"},
      Block{4, "cobblestone", "cobblestone.png"},
      Block{5, "oak wood plank", "oak_panks.png"},
      Block{12, "sand", "sand.png"},
      Block{17, "oak log", "oak_log.png"},
      Block{18, "oak leaves", "images/oak.png", true},
      Block{21, "lapis lazuli ore", "lapis_ore.png"},
      Block{24, "sandstone", "sandstone.png"},
      Block{35, "white wool", "white_wool.png"},
      Block{41, "gold block", "gold_block.png"},
      Block{44, "stone slab", "stone_slab.png"},
      Block{47, "bookshelf", "bookshelf.png"},
      Block{87, "nether rack", "netherrack.png"},
      Block{95, "white stained glass", "white_stained_glass.png"},
      Block{98, "stone bricks", "stone_bricks.png"},
      Block{103, "melon block", "melon_side.png"},
      Block{112, "nether block", "red_nether_bricks.png"},
      Block{129, "emerald ore", "emerald_ore.png"},
      Block{139, "cobblestone wall", "stone_bricks_wall.png"},
      Block{159, "white hardened clay", "clay.png"},
      Block{160, "white stained glass", "white_stained_glass.png"},
      Block{161, "acacia leaves", "images/oak.png", true},
      Block{168, "prismarine", "prismarine.png"},
      Block{173, "block of coal", "coal_block.png"},
      Block{179, "red sandstone", "red_sandstone.png"},
      Block{180, "wooden trapdoor", "birch_trapdoor.png"},
      Block{201, "purple block", "purple_concrete.png"},
      Block{202, "purple pillar", "purpur_pillar.png"},
      Block{235, "white glazed terracotta", "white_terracotta.png"},
      Block{237, "magenta glazed terracotta", "magenta_terracotta.png"},
      Block{247, "brown glazed terracotta", "brown_terracotta.png"},
      Block{251, "white concrete", "white_concrete.png"},
      Block{252, "white concrete powder", "white_concrete_powder.png"}
  };
  return make_shared<TexturePack>(textureDir, blocks);
};

TexturePack::TexturePack(string textureDir, vector<Block> blocks) : blocks(blocks), textureDir(textureDir) {
  for(int i = 0; i < blocks.size(); i++) {
    idToIndex[blocks[i].id] = i;
  }

  logger = make_shared<spdlog::logger>("TexturePack", fileSink);
  logger->set_level(spdlog::level::debug);
};



int TexturePack::textureIndexFromId(int id) {
  /*
  if(idToCounts.contains(id)) {
      countsMutex.lock();
      idToCounts[id]++;
      countsMutex.unlock();
  } else {
      countsMutex.lock();
      idToCounts[id] = 0;
      countsMutex.unlock();
  }
  */
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

void TexturePack::logCounts() {
  std::vector<std::pair<int, int>> sortedCounts(idToCounts.begin(), idToCounts.end());

  sort(sortedCounts.begin(), sortedCounts.end(),
       [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
         return a.second > b.second;
       });

  stringstream msg;
  msg << "block counts" << endl;
  for(auto count: sortedCounts) {
    msg << "{blockId:" << count.first << ", count:" << count.second << endl;
  }
  logger->debug(msg.str());
  logger->flush();
}
