all:
		cd build && cmake ..
		cmake --build build/

run:
		./build/worldGen $(ARGS)

taggart: all
		./build/worldGen taggartSquare.osm

buildings: all
		./build/worldGen taggartSquare.osm -b

streets: all
		./build/worldGen taggartSquare.osm -s
