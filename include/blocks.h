#pragma once
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
    string textureDir;
//}
  public:
    TexturePack(string textureDir, vector<Block> blocks);
    int textureIndexFromId(int id);
    vector<string> imageNames();
  };

  TexturePack initializeBasicPack();
}
