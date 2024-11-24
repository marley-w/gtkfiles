# Maintainer: Your Name <marleymrw@proton.me>
pkgname=hyprfiles-git
pkgver=1.1.gd55cf4e
pkgrel=1
pkgdesc="A minimalist file manager tailored for Hyprland"
arch=('x86_64')
url="https://github.com/marley-w/hyprfiles"
license=('MIT') # Adjust this if your project uses a different license
depends=('gtk3') # Add other dependencies here
makedepends=('git')
source=("git+https://github.com/marley-w/hyprfiles.git")
sha256sums=('SKIP')

pkgver() {
    cd "$srcdir/hyprfiles"
    git describe --tags | sed 's/^v//;s/-/./g'
}

build() {
    cd "$srcdir/hyprfiles"
    # Add build instructions if needed, e.g., compiling code
}

package() {
    cd "$srcdir/hyprfiles"
    install -Dm755 "hyprfiles" "$pkgdir/usr/bin/hyprfiles" # Adjust the source file path
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
    install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
}

