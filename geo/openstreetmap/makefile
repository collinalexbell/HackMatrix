all:
		cd build && cmake ..
		cmake --build build/

run:
		./build/worldGen $(ARGS)

taggart: all
		./build/worldGen taggartSquare.osm
