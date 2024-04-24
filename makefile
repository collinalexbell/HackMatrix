PROTO_DIR = protos
PROTO_FILES = $(wildcard $(PROTO_DIR)/*.proto)
PROTO_CPP_FILES = $(patsubst %.proto, %.pb.cc, $(PROTO_FILES))
PROTO_H_FILES = $(patsubst %.proto, %.pb.h, $(PROTO_FILES))
INCLUDES        = -Iinclude -I/usr/local/include -Iinclude/imgui
# add -p for profiling with gprof
LOADER_FLAGS = -march=native -funroll-loops
SQLITE_SOURCES = $(wildcard src/sqlite/*.cpp)
SQLITE_OBJECTS = $(patsubst src/sqlite/%.cpp, build/%.o, $(SQLITE_SOURCES))
ALL_OBJECTS = build/screen.o build/systems/Light.o build/components/Light.o build/main.o build/systems/Boot.o build/components/Bootable.o build/IndexPool.o build/WindowManager/Space.o build/systems/Move.o build/systems/ApplyTranslation.o build/systems/Derivative.o build/systems/Update.o build/systems/Intersections.o build/systems/Scripts.o build/components/Scriptable.o build/components/Parent.o build/components/RotateMovement.o build/components/Lock.o build/components/Key.o build/systems/KeyAndLock.o build/systems/Door.o build/systems/ApplyRotation.o build/persister.o build/engineGui.o build/entity.o build/renderer.o build/shader.o build/texture.o build/world.o build/camera.o build/api.o build/controls.o build/app.o build/WindowManager/WindowManager.o build/logger.o build/engine.o build/cube.o build/chunk.o build/mesher.o build/loader.o build/utility.o build/blocks.o build/dynamicObject.o build/assets.o build/model.o build/mesh.o build/imgui/imgui.o build/imgui/imgui_draw.o build/imgui/imgui_impl_opengl3.o build/imgui/imgui_widgets.o build/imgui/imgui_demo.o build/imgui/imgui_impl_glfw.o build/imgui/imgui_tables.o build/enkimi.o build/miniz.o src/api.pb.cc src/glad.c src/glad_glx.c $(SQLITE_OBJECTS)
LIBS = -lzmq -lX11 -lXcomposite -lXtst -lXext -lXfixes -lprotobuf -lspdlog -lfmt -Llib -lglfw -lGL -lpthread -lassimp -lsqlite3


all: FLAGS+=-O3 -g
all: include/protos/api.pb.h matrix trampoline build/diagnosis

matrix: $(ALL_OBJECTS)
	g++ -std=c++20 $(FLAGS) -g -o matrix $(ALL_OBJECTS) $(LIBS) $(INCLUDES)

trampoline: src/trampoline.cpp build/x-raise
	g++ -o trampoline src/trampoline.cpp

build/enkimi.o: src/enkimi.c
	g++ $(FLAGS) $(LOADER_FLAGS) -o build/enkimi.o -c src/enkimi.c $(INCLUDES) -lm -Wno-unused-result

build/miniz.o: src/miniz.c
	g++ $(FLAGS) $(LOADER_FLAGS) -o build/miniz.o -c src/miniz.c $(INCLUDES) -lm

build/renderer.o: src/renderer.cpp include/renderer.h include/texture.h include/shader.h include/world.h include/camera.h include/cube.h include/logger.h include/dynamicObject.h include/model.h include/WindowManager/Space.h include/components/Bootable.h include/components/Light.h include/screen.h
	g++  -std=c++20 $(FLAGS) -o build/renderer.o -c src/renderer.cpp $(INCLUDES)

build/IndexPool.o: include/IndexPool.h src/IndexPool.cpp
	g++  -std=c++20 $(FLAGS) -o build/IndexPool.o -c src/IndexPool.cpp $(INCLUDES)

build/main.o: src/main.cpp include/engine.h include/screen.h
	g++ -std=c++20  $(FLAGS) -o build/main.o -c src/main.cpp $(INCLUDES)

build/shader.o: src/shader.cpp include/shader.h
	g++ -std=c++20  $(FLAGS) -o build/shader.o -c src/shader.cpp $(INCLUDES)

build/texture.o: src/texture.cpp include/texture.h
	g++  -std=c++20 $(FLAGS) -o build/texture.o -c src/texture.cpp $(INCLUDES)

build/world.o: src/world.cpp include/world.h include/app.h include/camera.h include/cube.h include/chunk.h include/loader.h include/utility.h include/dynamicObject.h include/renderer.h include/worldInterface.h include/model.h include/systems/ApplyRotation.h
	g++ -std=c++20 -g $(FLAGS) -o build/world.o -c src/world.cpp $(INCLUDES)

build/camera.o: src/camera.cpp include/camera.h
	g++  -std=c++20 $(FLAGS) -o build/camera.o -c src/camera.cpp $(INCLUDES)

build/api.o: src/api.cpp include/api.h include/world.h include/logger.h include/protos/api.pb.h
	g++  -std=c++20 $(FLAGS) -o build/api.o -c src/api.cpp $(INCLUDES)

build/controls.o: src/controls.cpp include/controls.h include/camera.h include/WindowManager/WindowManager.h include/world.h
	g++  -std=c++20 $(FLAGS) -o build/controls.o -c src/controls.cpp $(INCLUDES)

build/app.o: src/app.cpp include/app.h include/screen.h
	g++  -std=c++20 $(FLAGS) -o build/app.o -c src/app.cpp $(INCLUDES) -Wno-narrowing

build/screen.o: src/screen.cpp include/screen.h 
	g++  -std=c++20 $(FLAGS) -o build/screen.o -c src/screen.cpp $(INCLUDES) -Wno-narrowing

build/WindowManager/WindowManager.o: src/WindowManager/WindowManager.cpp include/WindowManager/WindowManager.h include/controls.h include/logger.h include/world.h include/WindowManager/Space.h include/systems/Boot.h include/components/Bootable.h include/screen.h
	g++ -std=c++20 $(FLAGS) -o build/WindowManager/WindowManager.o -c src/WindowManager/WindowManager.cpp $(INCLUDES)

build/WindowManager/Space.o: src/WindowManager/Space.cpp include/WindowManager/Space.h include/loader.h include/app.h include/camera.h include/renderer.h include/screen.h
	g++ -std=c++20 $(FLAGS) -o build/WindowManager/Space.o -c src/WindowManager/Space.cpp $(INCLUDES)

build/logger.o: src/logger.cpp include/logger.h
	g++ -std=c++20 $(FLAGS) -o build/logger.o -c src/logger.cpp $(INCLUDES)

build/engine.o: src/engine.cpp include/engine.h include/api.h include/app.h include/camera.h include/controls.h include/renderer.h include/WindowManager/WindowManager.h include/world.h include/blocks.h include/assets.h include/entity.h include/model.h include/systems/Derivative.h include/components/Light.h
	g++ -std=c++20 $(FLAGS) -o build/engine.o -c src/engine.cpp $(INCLUDES)

build/cube.o: src/cube.cpp include/cube.h
	g++ -std=c++20 $(FLAGS) -o build/cube.o -c src/cube.cpp $(INCLUDES)

build/chunk.o: src/chunk.cpp include/chunk.h include/mesher.h
	g++ -std=c++20 $(FLAGS) -o build/chunk.o -c src/chunk.cpp $(INCLUDES)

build/mesher.o: src/mesher.cpp include/mesher.h
	g++ -std=c++20 $(FLAGS) -o build/mesher.o -c src/mesher.cpp $(INCLUDES)

build/loader.o: src/loader.cpp include/loader.h include/utility.h
	g++ -std=c++20 $(FLAGS) -o build/loader.o -c src/loader.cpp $(INCLUDES)

build/utility.o: src/utility.cpp include/utility.h
	g++ -std=c++20 $(FLAGS) -o build/utility.o -c src/utility.cpp $(INCLUDES)

build/blocks.o: src/blocks.cpp include/blocks.h
	g++ -std=c++20 $(FLAGS) -o build/blocks.o -c src/blocks.cpp $(INCLUDES)

build/dynamicObject.o: src/dynamicObject.cpp include/dynamicObject.h
	g++ -std=c++20 $(FLAGS) -o build/dynamicObject.o -c src/dynamicObject.cpp $(INCLUDES)

build/assets.o: src/assets.cpp include/assets.h
	g++ -std=c++20 $(FLAGS) -o build/assets.o -c src/assets.cpp $(INCLUDES)

build/model.o: src/model.cpp include/model.h include/mesh.h
	g++ -std=c++20 $(FLAGS) -o build/model.o -c src/model.cpp $(INCLUDES)

build/mesh.o: src/mesh.cpp include/mesh.h
	g++ -std=c++20 $(FLAGS) -o build/mesh.o -c src/mesh.cpp $(INCLUDES)

build/entity.o: src/entity.cpp include/entity.h
	g++ -std=c++20 $(FLAGS) -o build/entity.o -c src/entity.cpp $(INCLUDES)

build/engineGui.o: src/engineGui.cpp include/engineGui.h include/components/RotateMovement.h include/model.h include/systems/Update.h include/components/Bootable.h include/components/Light.h
	g++ -std=c++20 $(FLAGS) -o build/engineGui.o -c src/engineGui.cpp $(INCLUDES)

build/persister.o: src/persister.cpp include/persister.h
	g++ -std=c++20 $(FLAGS) -o build/persister.o -c src/persister.cpp $(INCLUDES)

build/systems/ApplyRotation.o: src/systems/ApplyRotation.cpp include/systems/ApplyRotation.h include/components/RotateMovement.h include/model.h
	g++ -std=c++20 $(FLAGS) -o build/systems/ApplyRotation.o -c src/systems/ApplyRotation.cpp $(INCLUDES)

build/systems/ApplyTranslation.o: src/systems/ApplyTranslation.cpp include/systems/ApplyTranslation.h include/components/TranslateMovement.h include/model.h
	g++ -std=c++20 $(FLAGS) -o build/systems/ApplyTranslation.o -c src/systems/ApplyTranslation.cpp $(INCLUDES)


build/systems/Move.o: src/systems/Move.cpp include/systems/Move.h include/components/TranslateMovement.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Move.o -c src/systems/Move.cpp $(INCLUDES)


build/systems/Door.o: src/systems/Door.cpp include/systems/Door.h include/components/Door.h include/entity.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Door.o -c src/systems/Door.cpp $(INCLUDES)

build/systems/KeyAndLock.o: src/systems/KeyAndLock.cpp include/systems/KeyAndLock.h include/components/Key.h include/components/Lock.h
	g++ -std=c++20 $(FLAGS) -o build/systems/KeyAndLock.o -c src/systems/KeyAndLock.cpp $(INCLUDES)

build/systems/Boot.o: src/systems/Boot.cpp include/systems/Boot.h include/components/Bootable.h include/entity.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Boot.o -c src/systems/Boot.cpp $(INCLUDES)

build/systems/Light.o: src/systems/Light.cpp include/systems/Light.h include/components/Light.h include/entity.h include/renderer.h include/model.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Light.o -c src/systems/Light.cpp $(INCLUDES)

build/components/Key.o: src/components/Key.cpp include/components/Key.h include/SQLPersisterImpl.h include/components/RotateMovement.h
	g++ -std=c++20 $(FLAGS) -o build/components/Key.o -c src/components/Key.cpp $(INCLUDES)

build/components/Lock.o: src/components/Lock.cpp include/components/Lock.h include/SQLPersisterImpl.h
	g++ -std=c++20 $(FLAGS) -o build/components/Lock.o -c src/components/Lock.cpp $(INCLUDES)

build/components/RotateMovement.o: src/components/RotateMovement.cpp include/components/RotateMovement.h
	g++ -std=c++20 $(FLAGS) -o build/components/RotateMovement.o -c src/components/RotateMovement.cpp $(INCLUDES)


build/components/Parent.o: src/components/Parent.cpp include/components/Parent.h
	g++ -std=c++20 $(FLAGS) -o build/components/Parent.o -c src/components/Parent.cpp $(INCLUDES)

build/components/Scriptable.o: src/components/Scriptable.cpp include/components/Scriptable.h
	g++ -std=c++20 $(FLAGS) -o build/components/Scriptable.o -c src/components/Scriptable.cpp $(INCLUDES)

build/components/Bootable.o: src/components/Bootable.cpp include/components/Bootable.h include/SQLPersisterImpl.h include/screen.h
	g++ -std=c++20 $(FLAGS) -o build/components/Bootable.o -c src/components/Bootable.cpp $(INCLUDES)

build/components/Light.o: src/components/Light.cpp include/components/Light.h
	g++ -std=c++20 $(FLAGS) -o build/components/Light.o -c src/components/Light.cpp $(INCLUDES)

build/systems/Scripts.o: src/systems/Scripts.cpp include/systems/Scripts.h include/components/Scriptable.h include/entity.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Scripts.o -c src/systems/Scripts.cpp $(INCLUDES)


build/systems/Intersections.o: src/systems/Intersections.cpp include/systems/Intersections.h include/components/BoundingSphere.h include/entity.h include/model.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Intersections.o -c src/systems/Intersections.cpp $(INCLUDES)

build/systems/Update.o: src/systems/Update.cpp include/systems/Update.h include/entity.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Update.o -c src/systems/Update.cpp $(INCLUDES)

build/systems/Derivative.o: src/systems/Derivative.cpp include/systems/Intersections.h include/entity.h include/model.h include/components/Scriptable.h
	g++ -std=c++20 $(FLAGS) -o build/systems/Derivative.o -c src/systems/Derivative.cpp $(INCLUDES)

# SQLite build logic
build/%.o: src/sqlite/%.cpp
	g++ -std=c++20 $(FLAGS) -c $< -o $@ $(INCLUDES)

include/protos/api.pb.h src/api.pb.cc: $(PROTO_FILES)
	protoc --cpp_out=./ protos/api.proto
	protoc --python_out=client_libs/python/hackMatrix/ protos/api.proto
	mv protos/api.pb.cc src
	mv protos/api.pb.h include/protos


build/x-raise: src/x-raise.c
	gcc -o build/x-raise src/x-raise.c -lX11

build/diagnosis: src/diagnosis.cpp
	g++ -o build/diagnosis src/diagnosis.cpp

build/db: src/db.cpp
	g++ $(FLAGS) -o build/db src/db.cpp -lpqxx

docs: game-design.md
	pandoc -s game-design.md -o index.html
	python -m http.server


#######################
######## Tests ########
#######################

BUILD_OBJECTS_FOR_TEST = build/api.o build/dynamicObject.o build/logger.o src/api.pb.cc build/chunk.o build/mesher.o build/cube.o
TEST_OBJECTS = build/testApi.o build/testDynamicObject.o build/testChunk.o

test: FLAGS+=-O0
test: $(TEST_OBJECTS) $(BUILD_OBJECTS_FOR_TEST)
	g++ -std=c++20 $(FLAGS) -o test $(TEST_OBJECTS) $(BUILD_OBJECTS_FOR_TEST) /usr/src/gtest/src/gtest_main.cc /usr/src/gtest/src/gtest-all.cc -fuse-ld=gold -I /usr/src/gtest $(INCLUDES) -L /usr/local/lib -lzmq -lspdlog -lfmt -lprotobuf -lglfw

build/testDynamicObject.o: build/dynamicObject.o tests/dynamicObject.cpp include/dynamicObject.h
	g++ -std=c++20 $(FLAGS) -o build/testDynamicObject.o -c tests/dynamicObject.cpp $(INCLUDES)

build/testApi.o: build/api.o tests/api.cpp include/api.h
	g++ -std=c++20 $(FLAGS) -o build/testApi.o -c tests/api.cpp $(INCLUDES)

build/testChunk.o: build/chunk.o tests/chunk.cpp include/chunk.h include/mesher.h include/cube.h
	g++ -std=c++20 $(FLAGS) -o build/testChunk.o -c tests/chunk.cpp $(INCLUDES)



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
	cloc include/ src/ py/ client_libs/js client_libs/python/hackMatrix/api.py levels/ --exclude-dir=imgui,glad,glm,octree,protos,stb,zmq,scns,__pychache__,node_modules --not-match-f=\.json > line-count
	sed -i '/github.com\/AlDanial\/cloc/d' line-count

clean:
	rm build/*.o 2> /dev/null || true
