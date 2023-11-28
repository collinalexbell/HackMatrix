#include <iostream>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/visitor.hpp>

using namespace std;

class MyHandler : public osmium::handler::Handler {
public:
  void way(const osmium::Way &way) {
    std::cout << "way " << way.id() << ": {\n";
    for (const osmium::Tag &t : way.tags()) {
      std::cout << t.key() << "=" << t.value() << '\n';
    }
    for (const osmium::NodeRef &nr : way.nodes()) {
      std::cout << "ref=" << nr.ref() << '\n';
    }
    std::cout << "}\n";
  }

  void node(const osmium::Node &node) {
    std::cout << "node " << node.id() << "," <<  node.location() << '\n';
  }
};

int main() {
  cout << "world generator" << endl;
  osmium::io::File input_file{"testMap.osm"};
  osmium::io::Reader reader{input_file};
  MyHandler handler;
  osmium::apply(reader, handler);
  reader.close();
}
