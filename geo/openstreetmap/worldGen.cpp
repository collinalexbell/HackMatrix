#include <iostream>
#include <osmium/io/any_input.hpp>

using namespace std;
int main() {
  cout << "world generator" << endl;
  osmium::io::File input_file{"testMap.osm"};
  osmium::io::Reader reader{input_file};
  osmium::io::Header header = reader.header();
  cout << header.size() << endl;
}
