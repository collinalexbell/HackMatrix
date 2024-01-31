#include "loader.h"
#include "enkimi.h"
#include "utility.h"
#include <future>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

Coordinate getMinecraftChunkPos(int matrixChunkX, int matrixChunkZ) {
  auto matrixChunkSize = Chunk::getSize();
  vector<int> minecraftChunkSize = {16,256,16};
  auto x = matrixChunkX*matrixChunkSize[0]/minecraftChunkSize[0];
  auto z = matrixChunkZ * matrixChunkSize[2] / minecraftChunkSize[2];
  return Coordinate{x, z};
}

Coordinate getRelativeMinecraftChunkPos(int minecraftChunkX, int minecraftChunkZ) {
  vector<int> regionSize = {32, 32};
  assert(ENKI_MI_REGION_CHUNKS_NUMBER == regionSize[0] * regionSize[1]);
  int x = abs(minecraftChunkX) % 32;
  int z = abs(minecraftChunkZ) % 32;
  if(minecraftChunkX < 0) {
    x = 31 - x;
  }
  if(minecraftChunkZ < 0) {
    z = 31 - z;
  }
  return Coordinate{x,z};
}

Coordinate getMinecraftRegion(int minecraftChunkX, int minecraftChunkZ) {
  vector<int> regionSize = {32, 32};
  assert(ENKI_MI_REGION_CHUNKS_NUMBER == regionSize[0]*regionSize[1]);
  int subtractorX = 0;
  int subtractorZ = 0;
  if(minecraftChunkX < 0) {
    subtractorX = 1;
  }
  if(minecraftChunkZ < 0) {
    subtractorZ = 1;
  }
  return Coordinate{
    minecraftChunkX/regionSize[0]-subtractorX,
    minecraftChunkZ/regionSize[0]-subtractorZ
  };
}

Coordinate getWorldChunkPosFromMinecraft(int minecraftChunkX, int minecraftChunkZ) {
  auto matrixChunkSize = Chunk::getSize();
  vector<int> minecraftChunkSize = {16,256,16};
  float xf = float(minecraftChunkX * minecraftChunkSize[0]) / float(matrixChunkSize[0]);
  if(xf < 0) {
    xf = floor(xf);
  }
  int x = xf;

  float zf = float(minecraftChunkZ * minecraftChunkSize[2]) / float(matrixChunkSize[2]);
  if(zf < 0) {
    zf = floor(zf);
  }
  int z = zf;
  return Coordinate{x, z};
}

std::vector<std::string> getFilesInFolder(const std::string &folderPath) {
  std::vector<std::string> files;

  for (const auto &entry : fs::directory_iterator(folderPath)) {
    if (fs::is_regular_file(entry.path())) {
      files.push_back(entry.path().filename().string());
    }
  }

  return files;
}

std::array<int, 2>
getCoordinatesFromRegionFilename(const std::string &filename) {
  std::array<int, 2> coordinates = {
      0, 0}; // Initialize coordinates with default values

  try {
    // Extracting X and Z coordinates from the filename
    size_t startPos = filename.find_first_of(".") + 1;
    size_t endPos = filename.find_last_of(".");

    std::string coordsSubstring = filename.substr(startPos, endPos - startPos);
    size_t period = coordsSubstring.find_first_of(".");

    coordinates[0] = std::stoi(coordsSubstring.substr(0, period));
    coordinates[1] = std::stoi(coordsSubstring.substr(period + 1));
  } catch (const std::exception &e) {
    std::cerr << "Exception occurred: " << e.what() << std::endl;
    // Handle any exceptions, or you can leave the coordinates as default (0, 0)
  }

  return coordinates;
}

Loader::Loader(string folderName, shared_ptr<blocks::TexturePack> texturePack):
  texturePack(texturePack) {
  auto fileNames = getFilesInFolder(folderName);
  for (auto fileName : fileNames) {
    auto coords = getCoordinatesFromRegionFilename(fileName);
    auto key = Coordinate(coords);
    regionFileNames[key] = folderName + fileName;
  }
}

vector<LoaderChunk> Loader::getRegion(Coordinate regionCoordinate) {
  // copy contents of loadRegion except for addCube (which get converted to push_back())
  // in loadRegion, iterate over the chunks and cubes: call addCube()


  vector<LoaderChunk> chunks;

  map<int, int> counts;
  enkiRegionFile regionFile;
  string path = regionFileNames[regionCoordinate];
  if(regionFiles.contains(regionCoordinate)) {
    regionFile = regionFiles[regionCoordinate];
  } else {
    FILE *fp = fopen(path.c_str(), "rb");
    regionFile = enkiRegionFileLoad(fp);
    regionFiles[regionCoordinate] = regionFile;
  }
  for (unsigned int chunk = 0; chunk < ENKI_MI_REGION_CHUNKS_NUMBER; chunk++) {
    enkiNBTDataStream stream;
    enkiInitNBTDataStreamForChunk(regionFile, chunk, &stream);
    if(stream.dataLength) {
      enkiChunkBlockData aChunk = enkiNBTReadChunk(&stream);
      enkiMICoordinate chunkOriginPos = enkiGetChunkOrigin(&aChunk); // y always 0
      int chunkXPos = chunkOriginPos.x;
      int chunkZPos = chunkOriginPos.z;
      LoaderChunk lChunk;
      lChunk.foreignChunkX = chunkXPos / 16;
      lChunk.foreignChunkY = 0;
      lChunk.foreignChunkZ = chunkZPos / 16;
      chunks.push_back(lChunk);
      for (int section = 0; section < ENKI_MI_NUM_SECTIONS_PER_CHUNK; ++section) {
        if (aChunk.sections[section]) {
          enkiMICoordinate sectionOrigin = enkiGetChunkSectionOrigin(&aChunk, section);
          enkiMICoordinate sPos;
          for (sPos.y = 0; sPos.y < ENKI_MI_SIZE_SECTIONS; ++sPos.y) {
            for (sPos.z = 0; sPos.z < ENKI_MI_SIZE_SECTIONS; ++sPos.z) {
              for (sPos.x = 0; sPos.x < ENKI_MI_SIZE_SECTIONS; ++sPos.x) {
                uint8_t voxel =
                    enkiGetChunkSectionVoxel(&aChunk, section, sPos);
                if (voxel) {
                  if (!counts.contains(voxel)) {
                    counts[voxel] = 1;
                  } else {
                    counts[voxel]++;
                  }
                }
                LoaderCube cube;
                cube.x = sPos.x + sectionOrigin.x;
                cube.y = sPos.y + sectionOrigin.y + 64;
                cube.z = sPos.z + sectionOrigin.z;
                bool shouldAdd = false;
                auto textureIndex = texturePack->textureIndexFromId(voxel);
                if(textureIndex >= 0) {
                  cube.blockType = textureIndex;
                  shouldAdd = true;
                }
                if(shouldAdd) {
                  chunks.back().cubePositions.push_back(cube);
                }
              }
            }
          }
        }
      }
    }
      //heights = reader.get_heightmap_at(x, z);
  }
  // Create a vector of pairs to sort by value (count)
  std::vector<std::pair<int, int>> sortedCounts(counts.begin(), counts.end());
  std::sort(sortedCounts.begin(), sortedCounts.end(),
            [](const auto &a, const auto &b) {
              return a.second > b.second; // Sort in descending order
            });
  int count = 0;
  for (const auto &entry : sortedCounts) {
    count++;
    if (count == 6) // Stop after printing the top 6
      break;
  }

  return chunks;
}

// Comparison function to sort by smallest x, z
bool sortByXZ(shared_ptr<Chunk> chunk1, shared_ptr<Chunk> chunk2) {
  auto pos1 = chunk1->getPosition();
  auto pos2 = chunk2->getPosition();
  //assert(pos1.x==pos2.x || pos1.z == pos2.z);
  if (pos1.x != pos2.x) {
    return pos1.x < pos2.x;
  } else {
    return pos1.z < pos2.z;
  }
}

future<deque<shared_ptr<Chunk>>> Loader::readNextChunkDeque(array<Coordinate, 2> chunkCoords) {

  array<Coordinate, 2> regionCoords = {
      getMinecraftRegion(chunkCoords[0].x, chunkCoords[0].z),
      getMinecraftRegion(chunkCoords[1].x, chunkCoords[1].z)};

  return async(
      launch::async,
      [this, chunkCoords, regionCoords]() -> deque<shared_ptr<Chunk>> {
        int startX;
        int endX;
        int startZ;
        int endZ;

        if (regionCoords[0].x < regionCoords[1].x) {
          startX = regionCoords[0].x;
          endX = regionCoords[1].x;
        } else {
          startX = regionCoords[1].x;
          endX = regionCoords[0].x;
        }

        if (regionCoords[0].z < regionCoords[1].z) {
          startZ = regionCoords[0].z;
          endZ = regionCoords[1].z;
        } else {
          startZ = regionCoords[1].z;
          endZ = regionCoords[0].z;
        }

        stringstream regionsDebug;
        regionsDebug << "regionCoords: ((" << startX << "," << startZ << "),"
                     << "(" << endX << "," << endZ << "))" << endl;

        int chunkStartX;
        int chunkEndX;
        int chunkStartZ;
        int chunkEndZ;

        if (chunkCoords[0].x < chunkCoords[1].x) {
          chunkStartX = chunkCoords[0].x;
          chunkEndX = chunkCoords[1].x;
        } else {
          chunkStartX = chunkCoords[1].x;
          chunkEndX = chunkCoords[0].x;
        }

        if (chunkCoords[0].z < chunkCoords[1].z) {
          chunkStartZ = chunkCoords[0].z;
          chunkEndZ = chunkCoords[1].z;
        } else {
          chunkStartZ = chunkCoords[1].z;
          chunkEndZ = chunkCoords[0].z;
        }

        // assert(startX == endX || startZ == endZ);

        deque<shared_ptr<Chunk>> nextChunkDeque;
        unordered_map<Coordinate, shared_ptr<Chunk>, CoordinateHash> nextChunks;
        for (int x = startX; x <= endX; x++) {
          for (int z = startZ; z <= endZ; z++) {

            Coordinate regionCoords{x, z};

            auto region = getRegion(regionCoords);
            for (auto chunk : region) {
              if (chunk.foreignChunkX >= chunkStartX &&
                  chunk.foreignChunkX <= chunkEndX &&
                  chunk.foreignChunkZ >= chunkStartZ &&
                  chunk.foreignChunkZ <= chunkEndZ) {
                auto worldChunkPos = getWorldChunkPosFromMinecraft(
                    chunk.foreignChunkX, chunk.foreignChunkZ);

                if (!nextChunks.contains(worldChunkPos)) {
                  shared_ptr<Chunk> toAdd =
                      make_shared<Chunk>(worldChunkPos.x, 0, worldChunkPos.z);
                  nextChunks[worldChunkPos] = toAdd;
                }
                for (auto cube : chunk.cubePositions) {
                  auto worldPos =
                      translateToWorldPosition(cube.x, cube.y, cube.z);
                  /*
                    TODO: This fails, will need to be fixed
                  assert(worldPos.chunkX == worldChunkPos.x &&
                         worldPos.chunkZ == worldChunkPos.z);
                  */
                  glm::vec3 pos{worldPos.x, worldPos.y, worldPos.z};
                  Cube c(pos, cube.blockType);
                  nextChunks[worldChunkPos]->addCube(c, worldPos.x, worldPos.y,
                                                     worldPos.z);
                }
              }
            }
          }
        }

        for (auto nextChunk : nextChunks) {
          nextChunkDeque.push_back(nextChunk.second);
        }

        std::sort(nextChunkDeque.begin(), nextChunkDeque.end(), sortByXZ);

        for(auto chunk: nextChunkDeque) {
          chunk->meshAsync();
        }

        return nextChunkDeque;
      });
}
