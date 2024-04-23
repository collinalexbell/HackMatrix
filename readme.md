# Building

## Library Installation

Before compiling and running the program, ensure that you have the following libraries installed on your Linux system:

- ZeroMQ (`libzmq`)
- X11 (`libX11`)
- Xcomposite (`libXcomposite`)
- Xtst (`libXtst`)
- Xext (`libXext`)
- Xfixes (`libXfixes`)
- Protocol Buffers (`libprotobuf`)
- spdlog (`libspdlog`)
- fmt (`libfmt`)
- GLFW (`libglfw`)
- OpenGL (`libGL`)
- pthread (`libpthread`)
- Assimp (`libassimp`)
- SQLite3 (`libsqlite3`)

To install these libraries, you can use your distribution's package manager. Here are the commands for some common distributions:

### Ubuntu or Debian

```bash
sudo apt-get install libzmq3-dev libx11-dev libxcomposite-dev libxtst-dev libxext-dev libxfixes-dev libprotobuf-dev libspdlog-dev libfmt-dev libglfw3-dev libgl-dev libassimp-dev libsqlite3-dev
```

### Fedora or CentOS

```bash
sudo dnf install zeromq-devel libX11-devel libXcomposite-devel libXtst-devel libXext-devel libXfixes-devel protobuf-devel spdlog-devel fmt-devel glfw-devel mesa-libGL-devel assimp-devel sqlite-devel
```

### Arch Linux

```bash
sudo pacman -S zeromq libx11 libxcomposite libxtst libxext libxfixes protobuf spdlog fmt glfw-x11 mesa assimp sqlite
```

Make sure to install these libraries before proceeding with the compilation and execution of the program. The program's build system will link against these libraries using the provided `LIBS` flags:

```makefile
LIBS = -lzmq -lX11 -lXcomposite -lXtst -lXext -lXfixes -lprotobuf -lspdlog -lfmt -Llib -lglfw -lGL -lpthread -lassimp -lsqlite3
```

Once the libraries are installed, you can compile and run the program as described in the compilation and execution sections of this README.

## Building the Program

To build the `matrix` executable, navigate to the project directory and run the following command:

```bash
make all
```

This command will compile the source code and link against the required libraries specified in the `LIBS` variable:

```makefile
LIBS = -lzmq -lX11 -lXcomposite -lXtst -lXext -lXfixes -lprotobuf -lspdlog -lfmt -Llib -lglfw -lGL -lpthread -lassimp -lsqlite3
```

The build process will generate the `matrix` executable in the current directory.

If you want to clean the build artifacts and remove the generated executable, you can run:

```bash
make clean
```

This command will delete the `matrix` executable and any intermediate object files, allowing you to start a fresh build.

Make sure you have the necessary libraries installed, as described in the [Library Installation](#library-installation) section, before running the `make` commands.

Once the program is built successfully, you can proceed to run the `matrix` executable as described in the usage section of this README.
