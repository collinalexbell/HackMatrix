all: matrix

matrix: main.o renderer.o
	g++ -o matrix renderer.o main.o  src/glad.c -lglfw -lGL -lpthread -Iinclude  -lGL -Iinclude

renderer.o: src/renderer.cpp include/renderer.h
	g++ -c src/renderer.cpp -Iinclude

main.o: src/main.cpp include/renderer.h
	g++ -c src/main.cpp -Iinclude
