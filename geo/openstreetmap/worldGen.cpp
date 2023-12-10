#include <iostream>
#include <iomanip>
#include <map>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/visitor.hpp>
#include <utility>
#include "protos/api.pb.h"
#include <zmq/zmq.hpp>
#include "building.h"
#include "voxelizer.h"
#include "coreStructs.h"

using namespace std;

int main(int argc, char** argv) {
  std::cout.precision(40);
  cout << "world generator" << endl;
  string fileName = "testMap.osm";
  if(argc > 1) {
    fileName = argv[1];
  }
  osmium::io::File input_file{fileName};
  osmium::io::Reader reader{input_file};
  Voxelizer voxelizer;
  osmium::apply(reader, voxelizer);
  voxelizer.printAllTags();
  voxelizer.voxelizeBuildings();
  //voxelizer.voxelizeStreets();
  reader.close();
  return 0;
}
