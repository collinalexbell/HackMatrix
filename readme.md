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

HackMatrix uses `wofi`.

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

- wayland-protocols (`wayland-protocols`) (also pulls in wayland core)
- wofi (`wofi`)
- Xwayland (`xwayland`)
- XCB libraries for the wlroots X11 backend and Xwayland (`xcb`, `xcb-*`)
- ZeroMQ (`libzmq`)
- Protocol Buffers (`libprotobuf`)
- spdlog (`libspdlog`)
- fmt (`libfmt`)
- EGL (`libEGL`)
- OpenGL ES 2 (`libGLESv2`)
- pthread (`libpthread`)
- Assimp (`libassimp`)
- SQLite3 (`libsqlite3`)
- Protobuf (`protobuf1`)
- Base development tools (`basedevel`)

To install these libraries, you can use your distribution's package manager. Here are the commands for some common distributions:

#### Crow
HackMatrix now tries `find_package(Crow)` first and, if that fails, automatically builds Crow from source at the pinned revision `c61a26e`.
If you want to force the source-build path or use an already checked out local Crow tree, configure CMake like this:

```bash
cmake -S . -B build -DMATRIX_FORCE_FETCH_CROW=ON -DMATRIX_CROW_SOURCE_DIR=/path/to/Crow
```

That local source override is mainly useful for offline builds or for testing the fallback path on machines that already have Crow installed globally.


#### Ubuntu or Debian

```bash
sudo apt-get install cmake wofi xwayland wayland-protocols protobuf-compiler build-essential libzmq3-dev libprotobuf-dev libspdlog-dev libfmt-dev libegl-dev libgles2-mesa-dev libassimp-dev libsqlite3-dev pkgconf libwayland-dev libxcb1-dev libxcb-composite0-dev libxcb-dri3-dev libxcb-ewmh-dev libxcb-errors-dev libxcb-icccm4-dev libxcb-present-dev libxcb-render0-dev libxcb-render-util0-dev libxcb-res0-dev libxcb-shm0-dev libxcb-xfixes0-dev libxcb-xinput-dev
```

#### Fedora or CentOS

```bash
sudo dnf install wayland-protocols wayland-devel xorg-x11-server-Xwayland wofi protobuf-compiler @development-tools zeromq-devel protobuf-devel spdlog-devel fmt-devel mesa-libEGL-devel mesa-libGLES-devel assimp-devel sqlite-devel libxcb-devel xcb-util-errors-devel xcb-util-renderutil-devel xcb-util-wm-devel
```

#### Arch Linux

I'm currently working on an issue with protobuf compilation errors for Arch. [This PR](https://github.com/collinalexbell/HackMatrix/pull/48) shows how to resolve the issue.
If you are on Arch and would like to help with a PR that I can get merged into master, try out [this PR](https://github.com/collinalexbell/HackMatrix/pull/55) and let me know in the PR comments if it works for you. It would be much appreciated!

```bash
sudo pacman -S --needed wofi wayland wayland-protocols xorg-xwayland protobuf base-devel zeromq spdlog fmt mesa assimp sqlite libxcb xcb-util-errors xcb-util-renderutil xcb-util-wm
```

On arch you can also install the AUR package, or use the PKGBUILD in distributions/arch/

Replace "yay" with your package manager.

```bash
yay -S hackmatrix-git
```

#### Gentoo
```bash
 sudo emerge --autounmask-write gui-apps/wofi dev-libs/wayland dev-libs/wayland-protocols x11-base/xwayland x11-libs/libxcb x11-libs/xcb-util-errors x11-libs/xcb-util-renderutil x11-libs/xcb-util-wm net-libs/zeromq dev-libs/protobuf dev-libs/spdlog dev-libs/libfmt media-libs/mesa dev-db/sqlite dev-libs/pthreadpool media-libs/assimp
```
> [!NOTE]
> you may have some issues with use flags and or masked packages. you will have to figure that out on your own system.

## Building

***warning*** this is a new Wayland build. It will be buggy, but I merged it to main regardless

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## Running
It is recommended to launch from tty. In the root directory of the project
Run `./launch` 

If you installed HackMatrix using the AUR package, run it with the command

```bash
hackmatrix
```

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

# Ideas
- Model real spaces, like the park in my city, put them into hack Matrix, and then improve the design and democratically vote (with money) what design should be implemented, then carry out design in the physical space

- A hack Matrix cryptocurrency that is used to fund various hack Matrix projects

# Bugs
- Scrolling when in WM mode (not focused on a workspace) is handled by an unfocused window, cause undefined state that is only recoverable by focusing and unfocusing a window.
