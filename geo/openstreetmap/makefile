all:
		cd build && cmake ..
		cmake --build build/

run:
		./build/worldGen $(ARGS)

taggart: all
		printf '\n\n\n\n\n'
		./build/worldGen taggartSquare.osm
