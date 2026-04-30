# Maintainer: flumpsi <flumpsi@outlook.com>

# NOTE! This package is compiled without a -j flag, since some users might prefer to set it on their own.
# If you would like to set the -j flag, please edit the PKGBUILD or use MAKEFLAGS e.g: MAKEFLAGS="-j$(nproc)"
pkgname=hackmatrix-git
pkgrel=1
pkgdesc="HackMatrix is a 3D Linux desktop environment (which can also be a game engine)"
pkgver=r1123.1.stable.r296.ge38a2aa
arch=('x86_64')
url="https://github.com/collinalexbell/HackMatrix"
license=('MIT')
provides=("hackmatrix")
conflicts=(
  "hackmatrix"
  "hackmatrix-bin"
)
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
  "wlroots-fix.patch"
  "HackMatrix.desktop"
  "matrix-script"
)
sha256sums=(
  'SKIP'
  'd28e18337c55bb5cfd5f6f8f6178e2da2fa1c08ffa50d037efdacf0cb7476ed7'
  'db55d2c5e3d828f867d3c5a43e5b95519a0dfcb4ca669c40d467f4395e3ec6e4'
  '6281237d97263d6e575d9ca9833687b4463cece257838676e1fc7b1db3a555a8'
)

prepare() {
  cd "$srcdir/HackMatrix"
  echo "Syncing submodules"
  git submodule update --init --recursive
  echo "Applying wlroots patch"
  patch -p0 < "$srcdir/wlroots-fix.patch"
}

build() {
  cd "$srcdir/HackMatrix"
  mkdir -pv build
  cd build
  cmake ..
  cmake --build .
}

package() {
  cd "$srcdir/HackMatrix"
  echo "Installing HackMatrix binaries"
  install -Dm755 "$srcdir/matrix-script" "$pkgdir/usr/bin/hackmatrix"
  install -Dm755 build/matrix "$pkgdir/usr/share/HackMatrix/hackmatrix-binary"
  install -Dm755 "$srcdir/HackMatrix.desktop" "$pkgdir/usr/share/applications/HackMatrix.desktop"
  install -Dm644 config.yaml "$pkgdir/usr/share/HackMatrix/config.yaml"
  install -d "$pkgdir/usr/share/HackMatrix"
  cp -rv vox shaders scripts db "$pkgdir/usr/share/HackMatrix/"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
