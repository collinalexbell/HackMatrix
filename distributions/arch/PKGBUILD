# NOTE! This package is compiled without a -j flag, since some users might prefer to set it on their own.
# If you would like to set the -j flag, please edit the PKGBUILD or use MAKEFLAGS e.g: MAKEFLAGS="-j$(nproc)"
pkgname=hackmatrix-git
pkgrel=1
pkgdesc="HackMatrix is a 3D Linux desktop environment (which can also be a game engine)"
pkgver=r1117.1.stable.r290.gba01822
arch=('x86_64')
url="https://github.com/collinalexbell/HackMatrix"
license=('MIT')
depends=(
  'wayland'
  'xorg-server'
  'xorg-xinit'
  'libx11'
  'libxcomposite'
  'libxtst'
  'libxext'
  'libxfixes'
  'mesa'
  'glfw-x11'
  'xdotool'
  'xorg-xrandr'
  'xorg-xwininfo'
  'sqlite'
  'zeromq'
  'assimp'
  'fmt'
  'spdlog'
  'wlroots0.19'
)

makedepends=(
  'base-devel'
  'cmake'
  'git'
  'wayland-protocols'
  'protobuf'
)

pkgver() {
  cd "$srcdir/HackMatrix"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git describe --long --tags --always | sed 's/\([^-]*-g\)/r\1/;s/-/./g;s/^v//')"
}
source=(
  "HackMatrix::git+https://github.com/collinalexbell/HackMatrix.git"
)
sha256sums=(
  'SKIP'
)

prepare() {
  cd "$srcdir/HackMatrix"
  echo "Syncing submodules"
  git submodule update --init --recursive
  echo "Applying wlroots patch"
  patch -p0 < ../../wlroots-fix.patch
}

build() {
  cd "$srcdir/HackMatrix"
  mkdir -pv build
  cd build
  cmake ..
  make
}

package() {
  cd "$srcdir/HackMatrix"
  echo "Installing HackMatrix binaries"
  install -Dm755 ../../matrix-script "$pkgdir/usr/bin/hackmatrix"
  install -Dm755 build/matrix "$pkgdir/usr/share/HackMatrix/hackmatrix-binary"
  install -Dm755 ../../HackMatrix.desktop "$pkgdir/usr/share/applications/HackMatrix.desktop"
  install -Dm644 config.yaml "$pkgdir/usr/share/HackMatrix/config.yaml"
  cp -r vox shaders scripts db "$pkgdir/usr/share/HackMatrix/"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
