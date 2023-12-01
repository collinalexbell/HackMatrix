#include <iostream>
#include <map>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/visitor.hpp>
#include <utility>

using namespace std;

namespace WorldGen {
  struct Node {
    int id;
    osmium::Location location;
  };

  struct Way {
    int id;
    vector<Node*> nodes;
    void addNodeRef(int nodeRef) {
      nodeRefs.push_back(nodeRef);
    }
  private:
    vector<int> nodeRefs;
  };

  class Voxelizer : public osmium::handler::Handler {
    // Handlers need to handle data in a stream, not just marshal them into memory
    vector<Way> ways;
    map<int, Node> nodes;
  public:
    void way(const osmium::Way &way) {
      Way w;
      std::cout << "way " << way.id() << ": {\n";
      w.id = way.id();
      for (const osmium::Tag &t : way.tags()) {
        std::cout << t.key() << "=" << t.value() << '\n';
      }
      for (const osmium::NodeRef &nr : way.nodes()) {
        std::cout << "ref=" << nr.ref() << '\n';
        w.addNodeRef(nr.ref());
      }
      std::cout << "}\n";
      ways.push_back(w);
    }

    void node(const osmium::Node &node) {
      Node n;
      n.id = node.id();
      n.location = node.location();
      std::cout << "node " << node.id() << "," <<  node.location() << '\n';
      nodes[n.id] = n;
    }

    vector<Way> getWays() {
      return ways;
    }
  };
}

int main() {
  cout << "world generator" << endl;
  osmium::io::File input_file{"testMap.osm"};
  osmium::io::Reader reader{input_file};
  WorldGen::Voxelizer voxelizer;
  osmium::apply(reader, voxelizer);
  cout << "# ways:" << voxelizer.getWays().size() << endl;
  reader.close();
  return 0;
}
