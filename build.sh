#!/bin/sh
# build.sh - build + package itop.
#
# On device: run as-is (uses /var/jb toolchain).
# In CI:     SDK=$(xcrun --sdk iphoneos --show-sdk-path) bash build.sh build
#            CLANG/LDID/DPKG default to the /var/jb paths on device and to the
#            active PATH elsewhere.
set -eu
umask 022

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SDK="${SDK:-/var/jb/usr/share/SDKs/iPhoneOS.sdk}"
CLANG="${CLANG:-/var/jb/usr/bin/clang}"
LDID="${LDID:-/var/jb/usr/bin/ldid}"
DPKG="${DPKG:-/var/jb/usr/bin/dpkg-deb}"
for tool in "$CLANG" "$LDID" "$DPKG"; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "missing tool: $tool (set CLANG/LDID/DPKG, or pass SDK for the headers)" >&2
        exit 1
    }
done
ACTION="${1:-build}"

VERSION="$(sed -n '/^Version:/{s/^Version:[[:space:]]*//;p;q;}' "$ROOT/packaging/control")"
[ -n "$VERSION" ] || { echo "packaging/control has no Version" >&2; exit 1; }
APP="$ROOT/build/itop.app"
PACKAGE="$ROOT/packages/itop_${VERSION}_iphoneos-arm64.deb"

build_itop() {
    rm -rf "$APP" "$ROOT/build/itop" "$ROOT/deb/var/jb/Applications/itop.app" "$PACKAGE"
    mkdir -p "$APP" "$ROOT/deb/var/jb/Applications" "$ROOT/deb/DEBIAN" "$ROOT/packages"

    "$CLANG" \
        -arch arm64 -mios-version-min=15.0 -isysroot "$SDK" \
        -fobjc-arc -fmodules -O2 -fvisibility=hidden \
        -Wno-unused-parameter -Wno-deprecated-declarations \
        -framework UIKit -framework Foundation -framework QuartzCore -framework CoreGraphics \
        -lproc -Wl,-dead_strip \
        -o "$APP/itop" "$ROOT/main.m"
    cp "$ROOT/Info.plist" "$APP/Info.plist"
    "$CLANG" -arch arm64 -mios-version-min=15.0 -isysroot "$SDK" \
        -O2 -o "$ROOT/build/make_icon" "$ROOT/tools/make_icon.c" -lz
    "$ROOT/build/make_icon" "$APP"
    chmod 755 "$APP" "$APP/itop"
    chmod 644 "$APP/Info.plist"
    "$LDID" -S"$ROOT/itop.entitlements" "$APP/itop"
    cp -R "$APP" "$ROOT/deb/var/jb/Applications/itop.app"

    cp "$ROOT/packaging/control" "$ROOT/deb/DEBIAN/control"
    cp "$ROOT/packaging/postinst" "$ROOT/deb/DEBIAN/postinst"

    chmod 755 "$ROOT/deb" "$ROOT/deb/DEBIAN" "$ROOT/deb/var" \
        "$ROOT/deb/var/jb" "$ROOT/deb/var/jb/Applications"
    find "$ROOT/deb/var/jb/Applications/itop.app" -type d -exec chmod 755 {} \;
    find "$ROOT/deb/var/jb/Applications/itop.app" -type f -exec chmod 644 {} \;
    chmod 755 "$ROOT/deb/var/jb/Applications/itop.app/itop"
    chmod 644 "$ROOT/deb/DEBIAN/control"
    chmod 755 "$ROOT/deb/DEBIAN/postinst"
    "$DPKG" -Zgzip --root-owner-group -b "$ROOT/deb" "$PACKAGE"
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
        "$DPKG" --info "$PACKAGE"
        ;;
    install)
        build_itop
        /var/jb/usr/bin/sudo /var/jb/usr/bin/dpkg -i "$PACKAGE"
        ;;
    clean)
        rm -rf "$ROOT/build" "$ROOT/deb" "$ROOT/packages"
        ;;
    *)
        echo "Usage: $0 [build|test|install|clean]" >&2
        exit 2
        ;;
esac
