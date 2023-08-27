all: matrix

matrix: main.o renderer.o
	g++ -o matrix renderer.o main.o  src/glad.c -lglfw -lGL -lpthread -Iinclude  -lGL -Iinclude

renderer.o: src/renderer.cpp include/renderer.h
	g++ -c src/renderer.cpp -Iinclude

main.o: src/main.cpp include/renderer.h
	g++ -c src/main.cpp -Iinclude

# checkout f135f82 to compile this properly
learn_opengl_5.8.1: src/exercises/learn_opengl_5.8.1.cpp include/renderer.h main.o
	g++ -c src/exercises/learn_opengl_5.8.1.cpp -Iinclude
	g++ -o learn_opengl_5.8.1 learn_opengl_5.8.1.o main.o src/glad.c -lglfw -lGL -lpthread -Iinclude  -lGL -Iinclude

clean:
	rm *.o
