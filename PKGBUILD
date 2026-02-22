# Maintainer: Marek Samec
pkgname=qt-msg-reader
pkgver=0.1.0
pkgrel=1
pkgdesc="A Qt-based application for reading Microsoft Outlook MSG files"
arch=('x86_64')
url="https://github.com/mareksamec/qt-msg-reader"
license=('MIT')
depends=('qt6-base' 'python')
# python-extract-msg needs to be installed as manually as it is AUR package:
optdepends=('python-extract-msg')
makedepends=('cmake' 'git')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$pkgname-$pkgver"
    mkdir -p build
    cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    make
}

package() {
    cd "$pkgname-$pkgver/build"
    make DESTDIR="$pkgdir" install
    
    # Install license
    install -Dm644 "$srcdir/$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
