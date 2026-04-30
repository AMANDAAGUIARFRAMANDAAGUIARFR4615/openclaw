#!/bin/bash

set -euo pipefail

if [ -z "${FFMPEG_DIR:-}" ]; then
  export FFMPEG_DIR="/Users/lin/Qt/6.10.2/android_arm64_v8a/lib/ffmpeg"
fi

if [ -z "${ANDROID_NDK_ROOT:-}" ] && [ -n "${ANDROID_NDK_HOME:-}" ]; then
  export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
fi

if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
  echo "❌ 未找到 ANDROID_NDK_ROOT，请先配置 Android NDK 环境变量。"
  exit 1
fi

HOST_TAG=""
if [ -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-arm64" ]; then
  HOST_TAG="darwin-arm64"
elif [ -d "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64" ]; then
  HOST_TAG="darwin-x86_64"
else
  echo "❌ 未找到可用的 NDK toolchain 目录。"
  exit 1
fi

TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG"
SYSROOT="$TOOLCHAIN/sysroot"

BUILD_DIR="$PWD/build"
OUTPUT_ROOT="$BUILD_DIR/output"
API_LEVEL="${ANDROID_API_LEVEL:-21}"
JOBS="$(sysctl -n hw.ncpu)"

LIBS=("libavcodec" "libavformat" "libavutil" "libswresample" "libswscale")
ABIS=("arm64-v8a")

mkdir -p "$OUTPUT_ROOT"

build_one_abi() {
  local ABI="$1"
  local ARCH=""
  local CPU=""
  local CC=""
  local EXTRA_CFLAGS="-fPIC -DPIC"
  local EXTRA_CXXFLAGS="-fPIC -DPIC"
  local EXTRA_LDFLAGS=""
  local EXTRA_CONFIG_FLAGS=()
  local PREFIX="$OUTPUT_ROOT/$ABI"

  case "$ABI" in
    arm64-v8a)
      ARCH="aarch64"
      CPU="armv8-a"
      CC="$TOOLCHAIN/bin/aarch64-linux-android${API_LEVEL}-clang"
      # Work around AArch64 non-PIC relocations from FFT NEON asm objects
      # (e.g. tx_float_neon.o) when linking into shared Qt plugins.
      EXTRA_CONFIG_FLAGS+=(--disable-asm --disable-neon)
      ;;
    *)
      echo "❌ 不支持的 ABI: $ABI"
      exit 1
      ;;
  esac

  echo "🤖 开始编译 Android ABI: $ABI"
  make distclean > /dev/null 2>&1 || true

  ./configure \
    --prefix="$PREFIX" \
    --target-os=android \
    --arch="$ARCH" \
    --cpu="$CPU" \
    --enable-cross-compile \
    --sysroot="$SYSROOT" \
    --cc="$CC" \
    --cxx="${CC/clang/clang++}" \
    --strip="$TOOLCHAIN/bin/llvm-strip" \
    --ar="$TOOLCHAIN/bin/llvm-ar" \
    --ranlib="$TOOLCHAIN/bin/llvm-ranlib" \
    --nm="$TOOLCHAIN/bin/llvm-nm" \
    --disable-programs \
    --disable-everything \
    --enable-pic \
    --enable-static \
    --disable-shared \
    --disable-doc \
    --enable-jni \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swresample \
    --enable-swscale \
    --enable-decoder=h264 \
    --enable-decoder=aac \
    --enable-parser=h264 \
    --enable-parser=aac \
    --enable-demuxer=h264 \
    --enable-demuxer=mov \
    --enable-protocol=file \
    --enable-protocol=tcp \
    --enable-filter=scale \
    --enable-filter=format \
    --disable-xlib \
    --disable-libxcb \
    --disable-zlib \
    "${EXTRA_CONFIG_FLAGS[@]}" \
    --extra-cflags="$EXTRA_CFLAGS" \
    --extra-cxxflags="$EXTRA_CXXFLAGS" \
    --extra-ldflags="$EXTRA_LDFLAGS" > /dev/null

  make -j"$JOBS" install > /dev/null
  echo "✅ 编译完成: $ABI"
}

for ABI in "${ABIS[@]}"; do
  build_one_abi "$ABI"
done

echo "📦 开始整理 FFmpeg 产物..."
mkdir -p "$FFMPEG_DIR/lib"
rm -rf "$FFMPEG_DIR/include"
cp -R "$OUTPUT_ROOT/arm64-v8a/include" "$FFMPEG_DIR/include"

for ABI in "${ABIS[@]}"; do
  mkdir -p "$FFMPEG_DIR/lib/$ABI"
  for LIB_NAME in "${LIBS[@]}"; do
    LIB_PATH="$OUTPUT_ROOT/$ABI/lib/${LIB_NAME}.a"
    if [ -f "$LIB_PATH" ]; then
      # Keep ABI-specific layout for Android packaging.
      cp "$LIB_PATH" "$FFMPEG_DIR/lib/$ABI/"
      # Also keep a flat layout so FindFFmpeg.cmake can resolve libs via FFMPEG_DIR/lib.
      cp "$LIB_PATH" "$FFMPEG_DIR/lib/"
    else
      echo "❌ 缺少库文件: $LIB_PATH"
      exit 1
    fi
  done
done

echo "🧹 清理临时文件..."
rm -rf "$BUILD_DIR"
make distclean > /dev/null 2>&1 || true

echo "✅ Android FFmpeg 构建完成，输出目录: $FFMPEG_DIR"
