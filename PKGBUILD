# Maintainer: Your Name <your.email@example.com>
pkgname=hyprfiles
pkgver=1.0
pkgrel=1
pkgdesc="A lightweight GTK3-based file manager"
arch=('x86_64')
url="https://github.com/marley-w/hyprfiles"  # Replace with your repo URL
license=('MIT')
depends=('gtk3' 'glib2')
makedepends=('git' 'pkg-config')
source=("git+$url.git")
sha256sums=('SKIP')

pkgver() {
    cd "$srcdir/$pkgname"
    git describe --tags --always | sed 's/^v//'
}

build() {
    cd "$srcdir/$pkgname"
    g++ hyprfiles.cpp -o hyprfiles `pkg-config --cflags --libs gtk+-3.0`
}

package() {
    cd "$srcdir/$pkgname"
    install -Dm755 hyprfiles "$pkgdir/usr/bin/hyprfiles"
}
