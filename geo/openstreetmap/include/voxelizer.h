#pragma once

#include "protos/api.pb.h"
#include <map>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/visitor.hpp>
#include <unistd.h>
#include <utility>
#include <zmq/zmq.hpp>
#include <vector>
#include "coreStructs.h"
#include "building.h"
#include "apiInterface.h"
#include "geo.h"

using namespace std;

struct Node {
  int id;
  osmium::Location location;
};

struct Way {
  int id;
  map<string,string> tags;
  vector<Node *> nodes;
  void addNodeRef(int nodeRef) { nodeRefs.push_back(nodeRef); }
  vector<int> nodeRefs;
};

class Voxelizer : public osmium::handler::Handler, public ApiInterface {
  // Handlers need to handle data in a stream, not just marshal them into memory
  bool wired = false;
  vector<Way> ways;
  map<int, Node> nodes;

  zmq::context_t context;
  zmq::socket_t socket;

public:
  Voxelizer() {
    socket = zmq::socket_t(context, zmq::socket_type::req);
    socket.connect("tcp://localhost:3333");
  }

  void addCube(int x, int y, int z, int blockType) override {
    AddCube cube;
    cube.set_x(x);
    cube.set_y(y);
    cube.set_z(z);
    cube.set_blocktype(blockType);

    // Create an instance of ApiRequest and set its type and payload
    ApiRequest request;
    request.set_type(MessageType::ADD_CUBE);
    request.mutable_addcube()->CopyFrom(cube);

    // Serialize the ApiRequest message to send over the socket
    std::string serializedRequest;
    if (request.SerializeToString(&serializedRequest)) {
      zmq::message_t message(serializedRequest.size());
      memcpy(message.data(), serializedRequest.data(),
             serializedRequest.size());

      // Send the serialized message
      socket.send(message, zmq::send_flags::none);
    } else {
      // Handle serialization failure
    }

    // Wait for the server response if needed
    zmq::message_t response;
    zmq::recv_result_t result =
      socket.recv(response, zmq::recv_flags::none);
  }
  void way(const osmium::Way &way) {
    Way w;
    w.id = way.id();
    for (const osmium::Tag &t : way.tags()) {
      w.tags[t.key()] = t.value();
    }
    for (const osmium::NodeRef &nr : way.nodes()) {
      w.addNodeRef(nr.ref());
    }
    ways.push_back(w);
  }

  void node(const osmium::Node &node) {
    Node n;
    n.id = node.id();
    n.location = node.location();
    nodes[n.id] = n;
  }

  vector<Way> getWays() {
    if(!wired) {
      for(auto &way: ways) {
        for(auto nodeRef: way.nodeRefs) {
          way.nodes.push_back(&nodes[nodeRef]);
        }
      }
    }
    return ways;
  }

  void printAllTags() {
    set<string> tags;
    for (auto way : getWays()) {
      for (auto tag = way.tags.begin(); tag != way.tags.end(); tag++) {
        tags.insert(tag->first);
      }
    }
    for (auto tag = tags.begin(); tag != tags.end(); tag++) {
      cout << *tag << endl;
    }
  }

  void voxelizeBuildings() {

vector<Building> buildings;
  set<string> buildingsVisited;
  set<string> toVisit;
  toVisit.insert("2724");
  toVisit.insert("2714");
  toVisit.insert("2202");
  toVisit.insert("2218");
  toVisit.insert("2136");
  toVisit.insert("2717");
  toVisit.insert("2725");
  //toVisit.insert("2735");
  toVisit.insert("2636");
  toVisit.insert("2133");
  for(auto way: getWays()) {
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
      building.draw(this);
    }
  }
  cout << "num buildings found: " << buildings.size() << endl;

  }
};

