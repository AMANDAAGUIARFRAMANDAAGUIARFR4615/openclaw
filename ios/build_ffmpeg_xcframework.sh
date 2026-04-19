#!/bin/bash

if [ -z "$FFMPEG_DIR" ]; then
  export FFMPEG_DIR="/Users/lin/Qt/6.10.2/ios/lib/ffmpeg"
fi

BUILD_DIR="$PWD/build"
OUTPUT_IOS="$BUILD_DIR/ios"
OUTPUT_SIM="$BUILD_DIR/simulator"

CLANG_PATH="$(xcrun -sdk iphoneos -find clang)"
SYSROOT_IOS="$(xcrun -sdk iphoneos --show-sdk-path)"
SYSROOT_SIM="$(xcrun -sdk iphonesimulator --show-sdk-path)"

# ================= 1. 编译真机版 (arm64) =================
echo "📱 1/3 开始编译 iOS 真机版 (arm64)..."
make clean > /dev/null 2>&1
FLAGS_IOS="-arch arm64 -miphoneos-version-min=12.0 -isysroot $SYSROOT_IOS"

./configure \
  --prefix="$OUTPUT_IOS" \
  --target-os=darwin \
  --arch=aarch64 \
  --enable-cross-compile \
  --sysroot="$SYSROOT_IOS" \
  --cc="$CLANG_PATH" \
  --as="$CLANG_PATH $FLAGS_IOS" \
  --extra-cflags="$FLAGS_IOS" \
  --extra-ldflags="$FLAGS_IOS" \
  --disable-programs --disable-everything --enable-static --disable-doc \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
  --enable-decoder=h264 --enable-decoder=aac \
  --enable-parser=h264 --enable-parser=aac \
  --enable-demuxer=h264 --enable-demuxer=mov \
  --enable-protocol=file --enable-protocol=tcp --enable-filter=scale --enable-filter=format \
  --disable-xlib --disable-libxcb > /dev/null

make -j$(sysctl -n hw.ncpu) install > /dev/null

# ================= 2. 编译模拟器版 (x86_64-simulator) =================
echo "💻 2/3 开始编译 iOS 模拟器版 (x86_64-simulator)..."
make clean > /dev/null 2>&1

SIM_TARGET="x86_64-apple-ios12.0-simulator"
FLAGS_SIM="-target $SIM_TARGET -arch x86_64 -miphonesimulator-version-min=12.0 -isysroot $SYSROOT_SIM"

./configure \
  --prefix="$OUTPUT_SIM" \
  --target-os=darwin \
  --arch=x86_64 \
  --enable-cross-compile \
  --sysroot="$SYSROOT_SIM" \
  --cc="$CLANG_PATH -target $SIM_TARGET" \
  --as="$CLANG_PATH $FLAGS_SIM" \
  --extra-cflags="$FLAGS_SIM" \
  --extra-ldflags="$FLAGS_SIM" \
  --disable-programs --disable-everything --enable-static --disable-doc \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
  --enable-decoder=h264 --enable-decoder=aac \
  --enable-parser=h264 --enable-parser=aac \
  --enable-demuxer=h264 --enable-demuxer=mov \
  --enable-protocol=file --enable-protocol=tcp --enable-filter=scale --enable-filter=format \
  --disable-xlib --disable-libxcb > /dev/null

make -j$(sysctl -n hw.ncpu) install > /dev/null

# ================= 3. 提取头文件 & 打包 XCFramework =================
echo "📦 3/3 开始处理头文件并生成 XCFramework..."

# 1. 提取头文件 (这是 CMake 能找到组件的关键！)
mkdir -p "$FFMPEG_DIR"
cp -r "$OUTPUT_IOS/include" "$FFMPEG_DIR/"
echo "✅ 头文件已复制到 $FFMPEG_DIR/include"

# 2. 打包库文件
LIBS=("libavcodec" "libavformat" "libavutil" "libswresample" "libswscale")

for LIB_NAME in "${LIBS[@]}"; do
    LIB_IOS="$OUTPUT_IOS/lib/${LIB_NAME}.a"
    LIB_SIM="$OUTPUT_SIM/lib/${LIB_NAME}.a"
    XCFRAMEWORK_PATH="$FFMPEG_DIR/${LIB_NAME}.xcframework"
    
    rm -rf "$XCFRAMEWORK_PATH"
    
    if [ -f "$LIB_IOS" ] && [ -f "$LIB_SIM" ]; then
        xcodebuild -create-xcframework -library "$LIB_IOS" -library "$LIB_SIM" -output "$XCFRAMEWORK_PATH" > /dev/null 2>&1
        echo "✅ 成功生成: ${LIB_NAME}.xcframework"
    else
        echo "❌ 打包失败: 缺少 $LIB_NAME 的 .a 文件"
    fi
done

# ================= 4. 打扫战场 =================
echo "🧹 清理临时文件..."
rm -rf "$BUILD_DIR"
make clean > /dev/null 2>&1
