#include <iostream>
#include <osmium/io/any_input.hpp>

using namespace std;
int main() {
  cout << "world generator" << endl;
  osmium::io::File input_file{"testMap.osm"};
  osmium::io::Reader reader{input_file};
  auto buffer = reader.read();
  osmium::io::Header header = reader.header();
  for (auto it = buffer.begin(); it != buffer.end(); it++) {
    cout << it->data() << endl;
  }
}
