all: main
main:
	g++ src/main.cpp src/glad.c -lglfw -lGL -lpthread -Iinclude -o matrix
