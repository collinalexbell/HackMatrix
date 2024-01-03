#include "loader.h"
#include "enkimi.h"
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

Loader::Loader(string folderName) {
  auto fileNames = getFilesInFolder(folderName);
  for (auto fileName : fileNames) {
    auto coords = getCoordinatesFromRegionFilename(fileName);
    auto key = Coordinate(coords);
    regionFiles[key] = folderName + fileName;
  }
}

vector<LoaderChunk> Loader::getRegion(Coordinate regionCoordinate) {
  // copy contents of loadRegion except for addCube (which get converted to push_back())
  // in loadRegion, iterate over the chunks and cubes: call addCube()


  vector<LoaderChunk> chunks;

  map<int, int> counts;
  string path = regionFiles[regionCoordinate];
  FILE *fp = fopen(path.c_str(), "rb");
  enkiRegionFile regionFile = enkiRegionFileLoad(fp);
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
                cube.y = sPos.y + sectionOrigin.y;
                cube.z = sPos.z + sectionOrigin.z;
                bool shouldAdd = false;
                if (voxel == 1) {
                  cube.blockType = 6;
                  shouldAdd = true;
                }
                if(voxel == 3) {
                  cube.blockType = 3;
                  shouldAdd = true;
                }
                if(voxel == 161) {
                  cube.blockType = 0;
                  shouldAdd = true;
                }
                if(voxel == 251) {
                  cube.blockType = 1;
                  shouldAdd = true;
                }
                if (voxel == 17) {
                  cube.blockType = 2;
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
