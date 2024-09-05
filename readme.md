# HackMatrix 

[Join the discord](https://discord.gg/Kx2rbJ8JCM)
[this is a fork of](https://github.com/collinalexbell/HackMatrix)
The purpose of this fork is just for me to see if I can get the installation to work seamlessly as it is still quite difficult.
The dependencies:
UBUNTU/DEBIAN
sudo apt-get install xdotool x11-utils protobuf-compiler build-essential libzmq3-dev libx11-dev libxcomposite-dev libxtst-dev libxext-dev libxfixes-dev libprotobuf-dev libspdlog-dev libfmt-dev libglfw3-dev libgl-dev libassimp-dev libsqlite3-dev
FEDORA/CENTOS
sudo dnf install xdotool xorg-x11-utils protobuf-compiler @development-tools zeromq-devel libX11-devel libXcomposite-devel libXtst-devel libXext-devel libXfixes-devel protobuf-devel spdlog-devel fmt-devel glfw-devel mesa-libGL-devel assimp-devel sqlite-devel
ARCH
sudo pacman -S xdotool dmenu xorg-server xorg-xwininfo xorg-xrandr protobuf base-devel zeromq libx11 libxcomposite libxtst libxext libxfixes spdlog fmt glfw-x11 mesa assimp sqlite
Clone the project (with submodules `git clone --recurse-submodules`), navigate to the project directory and run `make`:

Add the following line at the end of your `~/.xinitrc` file:

 cd ~/<replace with repository directory>
 exec ~/<replace with repository directory>/matrix
