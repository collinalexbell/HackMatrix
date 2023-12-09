#pragma once

#include "protos/api.pb.h"
#include <map>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/visitor.hpp>
#include <utility>
#include <zmq/zmq.hpp>
#include <vector>

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
        socket.recv(response, zmq::recv_flags::dontwait);
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
};
