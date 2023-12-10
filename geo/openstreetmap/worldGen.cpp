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

double distance(const osmium::Location &loc1,
                const osmium::Location &loc2) {
  constexpr double radius_earth = 6371000.0; // Earth's radius in meters

  // Convert latitude and longitude to radians
  double lat1 = loc1.lat() * M_PI / 180.0;
  double lon1 = loc1.lon() * M_PI / 180.0;
  double lat2 = loc2.lat() * M_PI / 180.0;
  double lon2 = loc2.lon() * M_PI / 180.0;

  // Calculate differences in latitude and longitude
  double dlat = lat2 - lat1;
  double dlon = lon2 - lon1;

  // Haversine formula to calculate distance
  double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
             cos(lat1) * cos(lat2) * sin(dlon / 2.0) * sin(dlon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  double distance = radius_earth * c;

  return distance;
}

AbsolutePosition getPosition(osmium::Location location) {
  osmium::Location origin;
  origin.set_lat(45.50295489999999887231751927174627780914);
  origin.set_lon(-122.6454810999999978093910613097250461578);
  osmium::Location latOnly = origin;
  latOnly.set_lat(location.lat());

  osmium::Location lonOnly = origin;
  lonOnly.set_lon(location.lon());

  double latDistance = distance(origin, latOnly);
  double lonDistance = distance(origin, lonOnly);

  double cubeSize = 0.25;

  AbsolutePosition rv;
  rv.z = latDistance / cubeSize;
  rv.x = lonDistance / cubeSize;
  rv.y = 6;

  return rv;
}



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
  vector<Building> buildings;
  set<string> buildingsVisited;
  set<string> toVisit;
  toVisit.insert("2724");
  toVisit.insert("2714");
  toVisit.insert("2734");
  for(auto way: voxelizer.getWays()) {
    string addr = way.tags["addr:housenumber"];
    for(auto tag = way.tags.begin(); tag != way.tags.end(); tag++) {
      for(auto addr: toVisit) {
        if(tag->second.find(addr) != string::npos) {
          cout << addr << ":" << tag->first << ":" << tag->second << ":" << addr.length() << endl;
        }
      }
    }
    if (addr.length() == 4 &&
        !buildingsVisited.contains(addr) &&
        toVisit.contains(addr)) {
      buildingsVisited.insert(way.tags["addr:housenumber"]);
      Building building;
      for(auto node: way.nodes) {
        AbsolutePosition pos = getPosition(node->location);
        if(building.size() < 4) {
          building.addCorner(pos);
        }
      }
      buildings.push_back(building);
      building.printCorners();
      voxelizer.drawBuilding(building);
    }
  }
  cout << "num buildings found: " << buildings.size();
  reader.close();
  return 0;
}
