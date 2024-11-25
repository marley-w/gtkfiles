# Maintainer: Marley <marleymrw@proton.me>
pkgname=hyprfiles
pkgver=1.0
pkgrel=1
pkgdesc="A lightweight GTK3-based file manager"
arch=('x86_64')
url="https://github.com/marley-w/hyprfiles"
license=('MIT')
depends=('gtk3' 'glib2' 'gcc' 'filesystem')
makedepends=('pkg-config')
source=("${pkgname}.cpp")
sha256sums=('SKIP')

build() {
    cd "$srcdir"
    g++ ${pkgname}.cpp -o ${pkgname} `pkg-config --cflags --libs gtk+-3.0`
}

package() {
    cd "$srcdir"
    install -Dm755 "${pkgname}" "$pkgdir/usr/bin/${pkgname}"
    install -Dm644 "${pkgname}.cpp" "$pkgdir/usr/share/doc/${pkgname}/${pkgname}.cpp"
}

