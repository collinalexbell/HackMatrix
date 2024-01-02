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
    stringstream ss;
    auto coords = getCoordinatesFromRegionFilename(fileName);
    auto key = Coordinate(coords);
    regionFiles[key] = folderName + fileName;
  }
}
