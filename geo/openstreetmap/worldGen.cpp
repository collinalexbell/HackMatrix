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
#include "voxelizer.h"

using namespace std;

namespace WorldGen {
}

int main(int argc, char** argv) {
  std::cout.precision(10);
  cout << "world generator" << endl;
  string fileName = "testMap.osm";
  if(argc > 1) {
    fileName = argv[1];
  }
  osmium::io::File input_file{fileName};
  osmium::io::Reader reader{input_file};
  Voxelizer voxelizer;
  osmium::apply(reader, voxelizer);
  voxelizer.addCube(10,6,-10,2);
  for(auto way: voxelizer.getWays()) {
    if(way.tags["addr:housenumber"] == "2724") {
      cout << "way at: ";
      for(auto node: way.nodes) {
        cout << "("
             << node->location.lat() << ","
             << node->location.lon() << "),";
      }
      cout << endl;
    }

  }
  reader.close();
  return 0;
}
