#!/bin/bash
# Build development-only SDL3 / LGPL FFmpeg libraries for one visionOS SDK.
set -euo pipefail
cd "$(dirname "$0")/.."
SDK=${1:-xrsimulator}
case "$SDK" in
    xros) TARGET=arm64-apple-xros2.0 ;;
    xrsimulator) TARGET=arm64-apple-xros2.0-simulator ;;
    *) echo "usage: $0 [xros|xrsimulator]" >&2; exit 2 ;;
esac
PREFIX="$(cd .. && pwd)/deps-$SDK"
SRC="$PWD/build/vision-deps/src"
JOBS=${LASVISION_JOBS:-8}
SYSROOT=$(xcrun --sdk "$SDK" --show-sdk-path)
mkdir -p "$SRC"

if [ ! -f "$PREFIX/lib/libSDL3.a" ]; then
    SDL_SRC="$SRC/SDL"
    if [ ! -d "$SDL_SRC/.git" ]; then
        git clone --depth 1 --branch release-3.4.18 https://github.com/libsdl-org/SDL.git "$SDL_SRC"
    fi
    git -C "$SDL_SRC" checkout --detach 829a65d769d935c4852f8159e964312c0957260a
    cmake -S "$SDL_SRC" -B "$SRC/sdl-$SDK" -G Ninja \
        -DCMAKE_SYSTEM_NAME=visionOS -DCMAKE_OSX_SYSROOT="$SDK" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=2.0 \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$SRC/sdl-$SDK" -j"$JOBS"
    cmake --install "$SRC/sdl-$SDK"
fi

if [ ! -f "$PREFIX/lib/libavformat.a" ]; then
    ARCHIVE="$SRC/ffmpeg-9.0.2.tar.xz"
    if [ ! -f "$ARCHIVE" ]; then
        curl -fL --max-time 180 https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz -o "$ARCHIVE.part"
        mv "$ARCHIVE.part" "$ARCHIVE"
    fi
    echo "8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e  $ARCHIVE" | shasum -a 256 -c
    FFMPEG_SRC="$SRC/ffmpeg-9.0.2"
    if [ ! -f "$FFMPEG_SRC/configure" ]; then
        mkdir -p "$FFMPEG_SRC"
        tar -xf "$ARCHIVE" -C "$FFMPEG_SRC" --strip-components=1
    fi
    mkdir -p "$SRC/ffmpeg-$SDK"
    (
        cd "$SRC/ffmpeg-$SDK"
        "$FFMPEG_SRC/configure" --prefix="$PREFIX" --cc=clang --cxx=clang++ \
            --arch=aarch64 --target-os=darwin --enable-cross-compile --sysroot="$SYSROOT" \
            --extra-cflags="-target $TARGET -isysroot $SYSROOT" \
            --extra-ldflags="-target $TARGET -isysroot $SYSROOT" \
            --enable-static --disable-shared --disable-asm --disable-autodetect \
            --disable-programs --disable-doc --disable-network --disable-avdevice \
            --disable-avfilter --disable-swresample --disable-everything \
            --enable-protocol=file --enable-demuxer=matroska,mov \
            --enable-decoder=vp9,vp8,vorbis,opus,h264,aac \
            --enable-parser=vp9,vp8,vorbis,opus,h264,aac
        make -j"$JOBS"
        make install
    )
fi
echo "Development dependencies ready: $PREFIX"
