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

class Voxelizer : public osmium::handler::Handler {
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

  void addCube(int x, int y, int z, int blockType) {
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

  void drawBuilding(Building building) {
    vector<AbsolutePosition> corners = building.getCorners();
    bool first = true;
    int x[2];
    int z[2];
    for(auto corner: corners) {
      if(first || corner.x < x[0]) {
        x[0] = corner.x;
      }
      if (first || corner.x > x[1]) {
        x[1] = corner.x;
      }
      if (first || corner.z < z[0]) {
        z[0] = corner.z;
      }
      if (first || corner.z > z[1]) {
        z[1] = corner.z;
      }
      first = false;
    }

    int height = 10;
    for(int y = 6; y < height+6; y++) {
      for(int xs=x[0]; xs<x[1]; xs++) {
        addCube(xs-650, y, z[0]-120, 2);
        addCube(xs-650, y, z[1]-120, 2);
      }

      for (int zs = z[0]; zs<z[1]; zs++) {
        addCube(x[0]-650, y, zs-120, 2);
        addCube(x[1]-650, y, zs-120, 2);
      }
    }
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
};

