#!/usr/bin/env bash
# Build and test Battery Inspector for arm64 on a host without Docker.
#
# Required environment variables:
#   ARM64_ROOTFS  Ubuntu 24.04 arm64 rootfs directory
#   PROOT         path to a proot binary
#   QEMU          path to qemu-aarch64-static
#
# The rootfs needs: cmake, g++, qtbase5-dev, qtdeclarative5-dev,
# qtdeclarative5-dev-tools, gettext, python3 and click.
#
# Example:
#   ARM64_ROOTFS=$HOME/ut-rootfs \
#   PROOT=/path/to/proot \
#   QEMU=/path/to/qemu-aarch64-static \
#   scripts/dev-build-arm64.sh
set -euo pipefail

: "${ARM64_ROOTFS:?set ARM64_ROOTFS to an Ubuntu 24.04 arm64 rootfs}"
: "${PROOT:?set PROOT to a proot binary}"
: "${QEMU:?set QEMU to qemu-aarch64-static}"

repo=$(cd "$(dirname "$0")/.." && pwd)

exec env PROOT_NO_SECCOMP=1 "$PROOT" -0 -q "$QEMU" \
    -r "$ARM64_ROOTFS" \
    -b /proc -b /dev -b /sys -b /etc/resolv.conf \
    -b "$repo:/build/battery-inspector" \
    -w /build/battery-inspector \
    /usr/bin/env -i \
    HOME=/root \
    PATH=/usr/sbin:/usr/bin:/sbin:/bin \
    LANG=C.UTF-8 \
    ARCH=arm64 \
    CLICK_FRAMEWORK=ubuntu-touch-24.04-1.x \
    APPARMOR_POLICY=2404.1 \
    bash -lc '
        set -e
        cmake -S . -B build-arm64 \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/ \
            -DARCH=arm64 \
            -DCLICK_FRAMEWORK=ubuntu-touch-24.04-1.x \
            -DAPPARMOR_POLICY=2404.1
        cmake --build build-arm64 -j4
        ctest --test-dir build-arm64 --output-on-failure
        rm -rf build-arm64/install
        DESTDIR="$PWD/build-arm64/install" cmake --install build-arm64
        click build build-arm64/install
    '
