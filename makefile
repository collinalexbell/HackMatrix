all: matrix

matrix: build/main.o build/renderer.o build/shader.o build/texture.o build/world.o build/camera.o build/api.o
	cd build
	g++ -g -o matrix build/renderer.o build/main.o build/shader.o build/texture.o build/world.o build/camera.o build/api.o src/glad.c -lglfw -lGL -lpthread -Iinclude  -lGL -lzmq -Iinclude

build/renderer.o: src/renderer.cpp include/renderer.h include/texture.h include/shader.h include/world.h include/camera.h
	g++ -g -o build/renderer.o -c src/renderer.cpp -Iinclude

build/main.o: src/main.cpp include/renderer.h include/camera.h
	g++ -g -o build/main.o -c src/main.cpp -Iinclude

build/shader.o: src/shader.cpp include/shader.h
	g++ -g -o build/shader.o -c src/shader.cpp -Iinclude

build/texture.o: src/texture.cpp include/texture.h
	g++ -g -o build/texture.o -c src/texture.cpp -Iinclude

build/world.o: src/world.cpp include/world.h
	g++ -g -o build/world.o -c src/world.cpp -Iinclude

build/camera.o: src/camera.cpp include/camera.h
	g++ -g -o build/camera.o -c src/camera.cpp -Iinclude

build/api.o: src/api.cpp include/api.h
	g++ -g -o build/api.o -c src/api.cpp -Iinclude

#######################
######## Utils ########
#######################

clean:
	rm build/*.o 2> /dev/null || true
