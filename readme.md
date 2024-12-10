# HackMatrix 

<img src="images/header_img.png" width="800">

A 3D Linux desktop environment (which can also be a game engine)

<a href="https://www.youtube.com/watch?v=L6xDqNhGeEM">Watch a demo</a>

[Join the discord](https://discord.gg/Kx2rbJ8JCM)

## Usage

### Navigate the 3d space
Look around with the mouse

<img src="vids/lookAround.gif" width="420">

Move with <br>
<img src="images/wasd.webp" width="100">

<img src="vids/move.gif" width="420">

### Open a window

HackMatrix uses `dmenu`.

`d` is mapped to movement, so press `v` without a modifier

Type your program name and press `<enter>`

The window will open up in the position you are looking at.

Press `r` to [focus](#manually-focus-window) on the window 

<img src="vids/openWindow.gif" width="420">

### Manually focus window
Look at the window and press `r`

__warning__: sometimes this doesn't work temporarily because of a bug.

If that happens just use Super+1 to focus a window and then manual focus will work again.


<img src="vids/focus.gif" width="420">

### Exit window
When focused on a window press `Super+e`

<img src="vids/exitWindow.gif" width="420">

### Window Hotkeys

Windows are auto hotkeyed in the order they are created.

`Super+<num>` to navigate

<img src="vids/hotkey.gif" width="420">

### Close a window

When window has focus, press `Super+q`

### Exit HackMatrix
When not focused on a window press `<esc>`

(press `Super+e` first if focused on a window)

### Take a screenshot

Press `p` to save a screenshot into the `<project_dir>/screenshots` folder

### Hackmatrix menu
There is a small menu at the top of HackMatrix.

You can use this to inspect and modify entities (the game engine aspect of HackMatrix)

When not focused on an app, press `f` to enter into mouse mode

Click the arrow at the left of the menu

Navigate to the Entity Editor

See the [wiki page](https://github.com/collinalexbell/HackMatrix/wiki/Game-Engine) for more info about the game engine and how to use the editor.

## Compilation/Installation

### Dependencies

Before compiling or running the program, ensure that you have the following libraries installed on your Linux system:

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
- XWinInfo (`x11-utils`)
- xdotool (`xdotool`)
- Protobuf (`protobuf1`)
- Base development tools (`basedevel`)

To install these libraries, you can use your distribution's package manager. Here are the commands for some common distributions:

#### Ubuntu or Debian

```bash
sudo apt-get install xdotool x11-utils protobuf-compiler build-essential libzmq3-dev libx11-dev libxcomposite-dev libxtst-dev libxext-dev libxfixes-dev libprotobuf-dev libspdlog-dev libfmt-dev libglfw3-dev libgl-dev libassimp-dev libsqlite3-dev pkgconf
```

#### Fedora or CentOS

```bash
sudo dnf install xdotool xorg-x11-utils protobuf-compiler @development-tools zeromq-devel libX11-devel libXcomposite-devel libXtst-devel libXext-devel libXfixes-devel protobuf-devel spdlog-devel fmt-devel glfw-devel mesa-libGL-devel assimp-devel sqlite-devel
```

#### Arch Linux

I'm currently working on an issue with protobuf compilation errors for Arch. [This PR](https://github.com/collinalexbell/HackMatrix/pull/48) shows how to resolve the issue.
If you are on Arch and would like to help with a PR that I can get merged into master, try out [this PR](https://github.com/collinalexbell/HackMatrix/pull/55) and let me know in the PR comments if it works for you. It would be much appreciated!

```bash
sudo pacman -S --needed xdotool dmenu xorg-server xorg-xinit xorg-xwininfo xorg-xrandr protobuf base-devel zeromq libx11 libxcomposite libxtst libxext libxfixes spdlog fmt glfw-x11 mesa assimp sqlite
```
#### Gentoo
```bash
 sudo emerge net-libs/zeromq x11-libs/libX11 x11-libs/libXcomposite x11-libs/libXtst x11-libs/libXext x11-libs/libXfixes dev-libs/protobuf dev-libs/spdlog dev-libs/libfmt glfw x11-libs/libGLw  dev-db/sqlite x11-misc/xdotool  protobuf dev-libs/pthreadpool media-libs/libass media-lib/assimp dmenu
```
> [!NOTE]
> you may have some issues with use flags and or masked packages. you will have to figure that out on your own system.

Make sure to install these libraries before proceeding with the compilation and execution of the program. The program's build system will link against these libraries using the provided `LIBS` flags:

```makefile
LIBS = -lzmq -lX11 -lXcomposite -lXtst -lXext -lXfixes -lprotobuf -lspdlog -lfmt -Llib -lglfw -lGL -lpthread -lassimp -lsqlite3
```

Once the libraries are installed, you can compile and run the program as described in the compilation and execution sections of this README.

### Installing

#### Compiling from source

Right now this is the only way to install the project.

#### Tracy submodule
Clone the project (with submodules `git clone --recurse-submodules`), navigate to the project directory and run `make`:

or

if you have already cloned the project, use 
```
git submodule update --init
```

The build process will generate the `matrix` executable in the current directory.

## Running

`matrix` is an X11 window manager, so it needs to be added to your X11 startup file

#### Standard

HackMatrix v1 is prone to crash, so you may want to run in [developer](#developer) mode to auto-restart HackMatrix if it crashes.

Add the following line at the end of your `~/.xinitrc` file:

 ```bash
 cd ~/<replace with repository directory>
 exec ~/<replace with repository directory>/matrix
 ```

#### Developer

When developing HackMatrix, I frequently quit and rerun the `matrix` program without restarting X.
I wrote a trampoline program that will restart HackMatrix every time you exit.
Unfortunately, if you use a bleeding edge distro like Arch, there is a resource leak in GLFW that prevents you from using this. I've submitted a patch PR and you can compile and install my [fork of GLFW](https://github.com/collinalexbell/glfw/tree/fixleak) if you wish to use the `trampoline`. Be sure to compile the `fixleak` branch, not `master`.

 ```bash
 cd ~/<replace with repository directory>
 exec ~/<replace with repository directory>/trampoline
 ```

To restart normally, just press `<esc>`

To exit to a terminal where you can manually start the program (to see stdout) or run a debugger press `<del>`

To exit the `trampoline`, run `pkill trampoline` in the terminal


##### How to use a debugger
- Press `<del>` in `trampoline` mode to escape to terminal
- Open a TTY with CTRL+FN+ALT+2
- Run `tmux` 
- Split the window `CTRL+b %`
- Run `<project root>/devtools/gdb` in one split (and start the program)
- Change to other split `CTLR+b <right arrow>`. Press `<enter>` to make sure the shell is accepting input.
- Run `<project root>/devtools/display` to go back to TTY1 (or CTRL+FN+ALT+1 if your machine lets you do that)


### Start X11 with startx

After you have edited your `~/.xinitrc` ([see this](#running)) just run `startx` to boot HackMatrix

### Start X11 with a graphical session manager

If you use something like GDM, you will have to create a .desktop file that calls `<project_dir>/matrix` or `<project_dir>`/trampoline.

See [this article](https://www.maketecheasier.com/customize-the-gdm-sessions-list/) for how to do that.

At some point I may install a session manager myself and I'll be able to write this desktop config.

If you create a working config yourself, it would be great if you PR'd it!

### How to get the client_libraries working
run `scripts/install-python-clientlib.sh` from the HackMatrix root to do an automated install

see the script below for what it runs
#### Python
```bash
# Install python hackMatrix lib
python -m venv hackmatrix_python
cd hackmatrix_python
source bin/activate
cd client_libs/python
pip install .
cd ../..

# Testing
python scripts/player-move.py
```


# Problems

## Trackpad disabled while typing (moving)
```
xinput list | grep -i touchpad
# grab the id and replace <id> below with it
xinput set-prop <id> "libinput Disable While Typing Enabled" 0
```
