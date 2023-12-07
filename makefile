PROTO_DIR = protos
PROTO_FILES = $(wildcard $(PROTO_DIR)/*.proto)
PROTO_CPP_FILES = $(patsubst %.proto, %.pb.cc, $(PROTO_FILES))
PROTO_H_FILES = $(patsubst %.proto, %.pb.h, $(PROTO_FILES))
INCLUDES        = -Iinclude -I/usr/local/include -Iinclude/imgui
FLAGS = -g
all: matrix trampoline build/diagnosis

matrix: build/main.o build/renderer.o build/shader.o build/texture.o build/world.o build/camera.o build/api.o build/controls.o build/app.o build/wm.o build/logger.o build/engine.o build/cube.o build/chunk.o build/mesher.o imgui_objects include/protos/api.pb.h src/api.pb.cc
	g++ -std=c++20 -g -o matrix build/renderer.o build/main.o build/shader.o build/texture.o build/world.o build/camera.o build/api.o build/controls.o build/app.o build/wm.o build/logger.o build/engine.o build/cube.o build/chunk.o build/mesher.o build/imgui/imgui.o build/imgui/imgui_draw.o build/imgui/imgui_impl_opengl3.o build/imgui/imgui_widgets.o build/imgui/imgui_demo.o build/imgui/imgui_impl_glfw.o build/imgui/imgui_tables.o src/api.pb.cc src/glad.c src/glad_glx.c -lglfw -lGL -lpthread -Iinclude -lzmq $(INCLUDES) -lX11 -lXcomposite -lXtst -lXext -lXfixes -lprotobuf -lspdlog -lfmt

trampoline: src/trampoline.cpp build/x-raise
	g++ -o trampoline src/trampoline.cpp

build/renderer.o: src/renderer.cpp include/renderer.h include/texture.h include/shader.h include/world.h include/camera.h include/cube.h include/logger.h
	g++  -std=c++20 $(FLAGS) -o build/renderer.o -c src/renderer.cpp $(INCLUDES)

build/main.o: src/main.cpp include/engine.h
	g++ -std=c++20  $(FLAGS) -o build/main.o -c src/main.cpp $(INCLUDES)

build/shader.o: src/shader.cpp include/shader.h
	g++ -std=c++20  $(FLAGS) -o build/shader.o -c src/shader.cpp $(INCLUDES)

build/texture.o: src/texture.cpp include/texture.h
	g++  -std=c++20 $(FLAGS) -o build/texture.o -c src/texture.cpp $(INCLUDES)

build/world.o: src/world.cpp include/world.h include/app.h include/camera.h include/cube.h include/chunk.h
	g++ -std=c++20 $(FLAGS) -O3 -o build/world.o -c src/world.cpp $(INCLUDES)

build/camera.o: src/camera.cpp include/camera.h
	g++  -std=c++20 $(FLAGS) -o build/camera.o -c src/camera.cpp $(INCLUDES)

build/api.o: src/api.cpp include/api.h include/world.h include/logger.h include/protos/api.pb.h
	g++  -std=c++20 $(FLAGS) -o build/api.o -c src/api.cpp $(INCLUDES)

build/controls.o: src/controls.cpp include/controls.h include/camera.h include/wm.h include/world.h
	g++  -std=c++20 $(FLAGS) -o build/controls.o -c src/controls.cpp $(INCLUDES)

build/app.o: src/app.cpp include/app.h
	g++  -std=c++20 $(FLAGS) -o build/app.o -c src/app.cpp $(INCLUDES) -Wno-narrowing

build/wm.o: src/wm.cpp include/wm.h include/controls.h include/logger.h include/world.h
	g++ -std=c++20 $(FLAGS) -o build/wm.o -c src/wm.cpp $(INCLUDES)

build/logger.o: src/logger.cpp include/logger.h
	g++ -std=c++20 $(FLAGS) -o build/logger.o -c src/logger.cpp $(INCLUDES)

build/engine.o: src/engine.cpp include/engine.h include/api.h include/app.h include/camera.h include/controls.h include/renderer.h include/wm.h include/world.h include/mesher.h
	g++ -std=c++20 $(FLAGS) -o build/engine.o -c src/engine.cpp $(INCLUDES)

build/cube.o: src/cube.cpp include/cube.h
	g++ -std=c++20 $(FLAGS) -o build/cube.o -c src/cube.cpp $(INCLUDES)

build/chunk.o: src/chunk.cpp include/chunk.h include/mesher.h
	g++ -std=c++20 $(FLAGS) -o build/chunk.o -c src/chunk.cpp $(INCLUDES)

build/mesher.o: src/mesher.cpp include/mesher.h
	g++ -std=c++20 $(FLAGS) -o build/mesher.o -c src/mesher.cpp $(INCLUDES)

include/protos/api.pb.h src/api.pb.cc: $(PROTO_FILES)
	protoc --cpp_out=./ protos/api.proto
	protoc --python_out=client_libs/python/hackMatrix/ protos/api.proto
	mv protos/api.pb.cc src
	mv protos/api.pb.h include/protos


build/x-raise: src/x-raise.c
	gcc -o build/x-raise src/x-raise.c -lX11

build/diagnosis: src/diagnosis.cpp
	g++ -o build/diagnosis src/diagnosis.cpp

docs: game-design.md
	pandoc -s game-design.md -o index.html
	python -m http.server

#######################
######## Imgui ########
#######################

# Source files
IMGUI_SRC = src/imgui/imgui.cpp src/imgui/imgui_draw.cpp src/imgui/imgui_tables.cpp src/imgui/imgui_widgets.cpp src/imgui/imgui_impl_opengl3.cpp src/imgui/imgui_impl_glfw.cpp src/imgui/imgui_demo.cpp

# Object files directory
IMGUI_OBJ_DIR = build/imgui

# Object files list based on source files
IMGUI_OBJ = $(patsubst src/imgui/%.cpp,$(IMGUI_OBJ_DIR)/%.o,$(IMGUI_SRC))

$(IMGUI_OBJ_DIR)/%.o: src/imgui/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ -I include/imgui

# Rule to compile all ImGui object files
imgui_objects: $(IMGUI_OBJ)

#######################
######## Utils ########
#######################

cloc:
	cloc include/ src/ --exclude-dir=imgui,glad,glm,octree,protos,stb,zmq > build/cloc

clean:
	rm build/*.o 2> /dev/null || true
