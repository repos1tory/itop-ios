#!/var/jb/bin/sh
set -eu
umask 022

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SDK="/var/jb/usr/share/SDKs/iPhoneOS.sdk"
APP="$ROOT/build/itop.app"
PACKAGE="$ROOT/itop_1.1.0_iphoneos-arm64.deb"
ACTION="${1:-build}"

build_itop() {
    rm -rf "$APP" "$ROOT/build/itop" "$ROOT/deb/var/jb/Applications/itop.app" "$PACKAGE"
    mkdir -p "$APP" "$ROOT/deb/var/jb/Applications"
    /var/jb/usr/bin/clang \
        -arch arm64 -mios-version-min=15.0 -isysroot "$SDK" \
        -fobjc-arc -fmodules -O2 -fvisibility=hidden \
        -Wno-unused-parameter -Wno-deprecated-declarations \
        -framework UIKit -framework Foundation -framework QuartzCore -framework CoreGraphics \
        -lproc -Wl,-dead_strip \
        -o "$APP/itop" "$ROOT/main.m"
    cp "$ROOT/Info.plist" "$APP/Info.plist"
    /var/jb/usr/bin/clang -arch arm64 -mios-version-min=15.0 -isysroot "$SDK" \
        -O2 -o "$ROOT/build/make_icon" "$ROOT/tools/make_icon.c" -lz
    "$ROOT/build/make_icon" "$APP"
    chmod 755 "$APP" "$APP/itop"
    chmod 644 "$APP/Info.plist"
    /var/jb/usr/bin/ldid -S"$ROOT/itop.entitlements" "$APP/itop"
    cp -R "$APP" "$ROOT/deb/var/jb/Applications/itop.app"
    chmod 755 "$ROOT/deb" "$ROOT/deb/DEBIAN" "$ROOT/deb/var" \
        "$ROOT/deb/var/jb" "$ROOT/deb/var/jb/Applications"
    find "$ROOT/deb/var/jb/Applications/itop.app" -type d -exec chmod 755 {} \;
    find "$ROOT/deb/var/jb/Applications/itop.app" -type f -exec chmod 644 {} \;
    chmod 755 "$ROOT/deb/var/jb/Applications/itop.app/itop"
    chmod 644 "$ROOT/deb/DEBIAN/control"
    chmod 755 "$ROOT/deb/DEBIAN/postinst"
    /var/jb/usr/bin/dpkg-deb -Zgzip --root-owner-group -b "$ROOT/deb" "$PACKAGE"
    echo "BUILT $PACKAGE"
    ls -lh "$APP/itop" "$PACKAGE"
}

case "$ACTION" in
    build)
        build_itop
        ;;
    test)
        build_itop
        "$APP/itop" --self-test
        /var/jb/usr/bin/dpkg-deb --info "$PACKAGE"
        ;;
    install)
        build_itop
        /var/jb/usr/bin/sudo /var/jb/usr/bin/dpkg -i "$PACKAGE"
        ;;
    clean)
        rm -rf "$ROOT/build" "$ROOT/deb/var/jb/Applications/itop.app" "$PACKAGE"
        ;;
    *)
        echo "Usage: $0 [build|test|install|clean]" >&2
        exit 2
        ;;
esac
