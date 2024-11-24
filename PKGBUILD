# Maintainer: Your Name <your.email@example.com>
pkgname=hyprfiles
pkgver=1.0.0
pkgrel=1
pkgdesc="A file manager for Hyprland"
arch=('x86_64')
url="https://github.com/marley-w/hyprfiles"
license=('MIT')
depends=('gtk3' 'git' 'other-dependencies-if-any')  # Add any other runtime dependencies here
makedepends=('git')  # Add build dependencies if needed
source=("git+https://github.com/marley-w/hyprfiles.git")
sha256sums=('SKIP')  # We skip the checksum because the source is from Git

pkgver() {
    cd "$srcdir/hyprfiles"  # Enter the hyprfiles directory
    echo "r$(git rev-list --count HEAD).$(git rev-parse --short HEAD)"  # Use commit count and hash
}

package() {
    cd "$srcdir/hyprfiles"  # Go into the hyprfiles directory

    # Install the executable (adjust this if your executable is in a different location)
    install -Dm755 hyprfiles "$pkgdir/usr/bin/hyprfiles"

    # Install other necessary files
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
    install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
}
