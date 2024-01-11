#pragma once
#include "logger.h"
#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <vector>


using namespace std;
namespace blocks {
  struct Block {
    int id;
    string name;
    string fileName;
    bool localFileName = false;
  };
  class TexturePack {
//{
 // this is const because I'm storing an index into blocks
    const vector<Block> blocks;
    map<int, int> idToIndex;
//}

    map<int, int> idToCounts;
    shared_ptr<spdlog::logger> logger;
    string textureDir;
    mutex countsMutex;
  public:
    TexturePack(string textureDir, vector<Block> blocks);
    int textureIndexFromId(int id);
    vector<string> imageNames();
    void logCounts();
  };

  shared_ptr<TexturePack> initializeBasicPack();
}
