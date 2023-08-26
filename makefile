all: matrix

matrix: main.o world.o
	g++ -o matrix world.o main.o  src/glad.c -lglfw -lGL -lpthread -Iinclude  -lGL -Iinclude

world.o: src/world.cpp include/world.h
	g++ -c src/world.cpp -Iinclude

main.o: src/main.cpp include/world.h
	g++ -c src/main.cpp -Iinclude
