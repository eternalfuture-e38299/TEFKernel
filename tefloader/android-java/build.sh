#!/bin/bash

# ===========================================
# JAR + DEX 完整构建脚本
# ===========================================

set -e  # 遇到错误立即退出

# 加载环境配置
if [ -f "config.env" ]; then
    source config.env
else
    # 默认配置
    JAVA_HOME=$(dirname $(dirname $(readlink -f $(which javac))))
    ANDROID_SDK="$HOME/Android/Sdk"
    BUILD_TOOLS="36.1.0"
    PLATFORM="android-36.1"
    MIN_API=24

    JAVAC="$JAVA_HOME/bin/javac"
    JAR="$JAVA_HOME/bin/jar"
    D8="$ANDROID_SDK/build-tools/$BUILD_TOOLS/d8"

    PROJECT_ROOT=$(pwd)
    SRC_DIR="$PROJECT_ROOT/src"
    BUILD_DIR="$PROJECT_ROOT/build"
    OUTPUT_DIR="$BUILD_DIR/output"
    LIBS_DIR="$BUILD_DIR/libs"
    ANDROID_JAR="$ANDROID_SDK/platforms/$PLATFORM/android.jar"
fi

# 创建目录
mkdir -p "$BUILD_DIR/classes" "$OUTPUT_DIR" "$LIBS_DIR"

echo "==========================================="
echo "JAR + DEX 构建系统"
echo "==========================================="
echo "项目根目录: $PROJECT_ROOT"
echo "源代码目录: $SRC_DIR"
echo "构建目录: $BUILD_DIR"
echo "输出目录: $OUTPUT_DIR"
echo "Java 编译器: $JAVAC"
echo "Android SDK: $ANDROID_SDK"
echo "Build Tools: $BUILD_TOOLS"
echo "Min API: $MIN_API"
echo "==========================================="
echo ""

# ===========================================
# 步骤 1: 编译 Java 源代码
# ===========================================
echo "步骤 1: 编译 Java 源代码"

# 查找所有 Java 源文件
SOURCE_FILES=$(find "$SRC_DIR" -name "*.java" | tr '\n' ' ')

if [ -z "$SOURCE_FILES" ]; then
    echo "错误: 未找到 Java 源文件"
    exit 1
fi

echo "找到 Java 源文件:"
echo "$SOURCE_FILES" | tr ' ' '\n' | sed 's/^/  /'
echo ""

# 构建 classpath
CLASSPATH="$ANDROID_JAR"
if [ -d "$LIBS_DIR" ]; then
    for lib in "$LIBS_DIR"/*.jar; do
        if [ -f "$lib" ]; then
            CLASSPATH="$CLASSPATH:$lib"
        fi
    done
fi

echo "Classpath:"
echo "$CLASSPATH" | tr ':' '\n' | sed 's/^/  /'
echo ""

# 编译 Java 文件
echo "正在编译 Java 文件..."
$JAVAC \
    -d "$BUILD_DIR/classes" \
    -cp "$CLASSPATH" \
    -source 17 \
    -target 17 \
    -encoding UTF-8 \
    $SOURCE_FILES

if [ $? -eq 0 ]; then
    echo "✓ Java 编译成功"
    echo "  类文件数量: $(find "$BUILD_DIR/classes" -name "*.class" | wc -l)"
else
    echo "✗ Java 编译失败"
    exit 1
fi

# ===========================================
# 步骤 2: 创建 JAR 文件
# ===========================================
echo ""
echo "步骤 2: 创建 JAR 文件"

# 进入 classes 目录创建 JAR
cd "$BUILD_DIR/classes"

# 创建 MANIFEST.MF（可选）
cat > MANIFEST.MF << EOF
Manifest-Version: 1.0
Created-By: JAR + DEX Build System
Main-Class: eternal.future.tefkernel.MainActivity
EOF

# 创建 JAR 文件
$JAR cvfm "$OUTPUT_DIR/tefloader.jar" MANIFEST.MF .

if [ $? -eq 0 ]; then
    JAR_SIZE=$(ls -lh "$OUTPUT_DIR/tefloader.jar" | awk '{print $5}')
    echo "✓ JAR 创建成功"
    echo "  文件: $OUTPUT_DIR/tefloader.jar"
    echo "  大小: $JAR_SIZE"
else
    echo "✗ JAR 创建失败"
    exit 1
fi

# 返回项目根目录
cd "$PROJECT_ROOT"

# ===========================================
# 步骤 3: 转换为 DEX
# ===========================================
echo ""
echo "步骤 3: 转换为 DEX"

# 检查 d8 工具
if [ ! -f "$D8" ]; then
    echo "错误: 找不到 d8 工具"
    echo "路径: $D8"
    echo ""
    echo "请安装 Android SDK Build Tools:"
    echo "  sdkmanager 'build-tools;$BUILD_TOOLS'"
    exit 1
fi

# 构建依赖库参数
LIB_FILES=""
if [ -d "$LIBS_DIR" ]; then
    for lib in "$LIBS_DIR"/*.jar; do
        if [ -f "$lib" ]; then
            LIB_FILES="$LIB_FILES $lib"
        fi
    done
fi

echo "输入文件:"
echo "  - $OUTPUT_DIR/tefloader.jar"
if [ -n "$LIB_FILES" ]; then
    echo "  - 依赖库:"
    for lib in $LIB_FILES; do
        echo "    - $lib"
    done
fi
echo ""

# 执行 D8 转换
echo "正在转换为 DEX..."
$D8 \
    --release \
    --min-api $MIN_API \
    --lib "$ANDROID_JAR" \
    --output "$OUTPUT_DIR" \
    "$OUTPUT_DIR/tefloader.jar" \
    $LIB_FILES

if [ $? -eq 0 ] && [ -f "$OUTPUT_DIR/classes.dex" ]; then
    # 重命名 DEX 文件
    mv "$OUTPUT_DIR/classes.dex" "$OUTPUT_DIR/tefloader.dex"

    DEX_SIZE=$(ls -lh "$OUTPUT_DIR/tefloader.dex" | awk '{print $5}')
    echo "✓ DEX 转换成功"
    echo "  文件: $OUTPUT_DIR/tefloader.dex"
    echo "  大小: $DEX_SIZE"
else
    echo "✗ DEX 转换失败"
    exit 1
fi

# ===========================================
# 步骤 4: 验证输出文件
# ===========================================
echo ""
echo "步骤 4: 验证输出文件"

# 检查 DEX 文件是否有效
echo "输出目录内容 ($OUTPUT_DIR):"
ls -lh "$OUTPUT_DIR/" | tail -n +2 | sed 's/^/  /'

echo ""
echo "文件信息:"
echo "  1. JAR 文件:"
echo "    - MD5:    $(md5sum "$OUTPUT_DIR/tefloader.jar" | cut -d' ' -f1)"
echo "    - SHA256: $(sha256sum "$OUTPUT_DIR/tefloader.jar" | cut -d' ' -f1)"
echo ""
echo "  2. DEX 文件:"
echo "    - MD5:    $(md5sum "$OUTPUT_DIR/tefloader.dex" | cut -d' ' -f1)"
echo "    - SHA256: $(sha256sum "$OUTPUT_DIR/tefloader.dex" | cut -d' ' -f1)"

# 如果有 dexdump，显示 DEX 详细信息
if command -v "$ANDROID_SDK/build-tools/$BUILD_TOOLS/dexdump" &> /dev/null; then
    echo ""
    echo "DEX 文件结构摘要:"
    "$ANDROID_SDK/build-tools/$BUILD_TOOLS/dexdump" -f "$OUTPUT_DIR/tefloader.dex" | \
        grep -E "(checksum|file_size|class_defs_size|method_ids_size|string_ids_size)" | \
        sed 's/^/    - /'
fi

echo ""
echo "==========================================="
echo "构建完成!"
echo "==========================================="
echo "输出文件:"
echo "  1. JAR: $OUTPUT_DIR/tefloader.jar"
echo "  2. DEX: $OUTPUT_DIR/tefloader.dex"
echo ""
echo "下一步:"
echo "  1. 使用 APK 工具将 DEX 打包进 APK"
echo "  2. 签名 APK 文件"
echo "  3. 安装到 Android 设备"
echo "==========================================="
