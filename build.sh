#!/bin/bash

set -e

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Script directory: $SCRIPT_DIR"

# 定义目录（基于脚本目录）
BUILD_DEBUG_DIR="$SCRIPT_DIR/build/cmake_debug"
BUILD_RELEASE_DIR="$SCRIPT_DIR/build/cmake_release"
OUTPUT_DIR="$SCRIPT_DIR/build"

# 从 CMakeLists.txt 中提取项目名称
PROJECT_NAME=$(grep -E '^project\s*\(' "$SCRIPT_DIR/CMakeLists.txt" | sed -E 's/^project\s*\(\s*([^[:space:]]+).*/\1/')
if [ -z "$PROJECT_NAME" ]; then
    PROJECT_NAME="DEFAULT_PROJECT_NAME"  # 默认值
    echo "Warning: Could not extract project name from CMakeLists.txt, using default: $PROJECT_NAME"
else
    echo "Found project name: $PROJECT_NAME"
fi


# 默认构建两个版本
BUILD_DEBUG=true
BUILD_RELEASE=true

# 时间统计变量
DEBUG_BUILD_TIME=0
RELEASE_BUILD_TIME=0
TOTAL_START_TIME=0

# 解析参数
for arg in "$@"; do
    case $arg in
        --debug)
            BUILD_RELEASE=false
            shift
            ;;
        --release)
            BUILD_DEBUG=false
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --debug     Build only Debug version"
            echo "  --release   Build only Release version"
            echo "  --help, -h  Show this help message"
            echo ""
            echo "If no options provided, builds both Debug and Release versions"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== Build Configuration ==="
echo "Build Debug: $BUILD_DEBUG"
echo "Build Release: $BUILD_RELEASE"
echo "Working directory: $(pwd)"
echo ""

# 记录总开始时间
TOTAL_START_TIME=$(date +%s)

# 设置 Git 安全目录（关键修复）
echo "=== Setting Git safe directory ==="
if command -v git >/dev/null 2>&1; then
    echo "Setting Git safe directory: $SCRIPT_DIR"
    git config --global --add safe.directory "$SCRIPT_DIR"
    # 也设置当前工作目录为安全目录（如果有的话）
    if [ "$(pwd)" != "$SCRIPT_DIR" ]; then
        git config --global --add safe.directory "$(pwd)" || true
    fi
    echo "✓ Git safe directory configured"
else
    echo "⚠ Git not found, skipping safe directory configuration"
fi
echo ""


# 清理并创建目录
echo "=== Cleaning and creating directories ==="
rm -rf "$BUILD_DEBUG_DIR" "$BUILD_RELEASE_DIR" 
mkdir -p "$BUILD_DEBUG_DIR" "$BUILD_RELEASE_DIR" "$OUTPUT_DIR"

# 构建 Debug 版本
if [ "$BUILD_DEBUG" = true ]; then
    echo "=== Building Debug version ==="
    DEBUG_START_TIME=$(date +%s)
    
    # 保存当前目录
    CURRENT_DIR=$(pwd)
    
    # 进入构建目录
    cd "$BUILD_DEBUG_DIR"
    echo "Building in: $(pwd)"
    
    # 使用脚本所在目录作为CMake源目录
    cmake "$SCRIPT_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build .
    
    # 返回原目录
    cd "$CURRENT_DIR"
    
    # 复制 Debug 版本 - 放宽查找条件
    DEBUG_FILE=$(find "$BUILD_DEBUG_DIR" -maxdepth 1 -name "$PROJECT_NAME-*" -type f | head -1)
    if [ -n "$DEBUG_FILE" ] && [ -f "$DEBUG_FILE" ]; then
        cp "$DEBUG_FILE" "$OUTPUT_DIR/"
        echo "✓ Debug version copied: $OUTPUT_DIR/$(basename "$DEBUG_FILE")"
    else
        echo "✗ Error: Debug build failed - executable not found"
        echo "Looking for: $BUILD_DEBUG_DIR/$PROJECT_NAME-*"
        echo "Available files in $BUILD_DEBUG_DIR:"
        ls -la "$BUILD_DEBUG_DIR" 2>/dev/null || true
        exit 1
    fi
    
    DEBUG_END_TIME=$(date +%s)
    DEBUG_BUILD_TIME=$((DEBUG_END_TIME - DEBUG_START_TIME))
    debug_hours=$((DEBUG_BUILD_TIME / 3600))
    debug_min=$(( (DEBUG_BUILD_TIME % 3600) / 60 ))
    debug_sec=$((DEBUG_BUILD_TIME % 60))
    
    if [ $debug_hours -gt 0 ]; then
        echo "  Build time: ${debug_hours} h ${debug_min} min ${debug_sec} sec"
    elif [ $debug_min -gt 0 ]; then
        echo "  Build time: ${debug_min} min ${debug_sec} sec"
    else
        echo "  Build time: ${debug_sec} sec"
    fi
fi

# 构建 Release 版本
if [ "$BUILD_RELEASE" = true ]; then
    echo "=== Building Release version ==="
    RELEASE_START_TIME=$(date +%s)
    
    # 保存当前目录
    CURRENT_DIR=$(pwd)
    
    # 进入构建目录
    cd "$BUILD_RELEASE_DIR"
    echo "Building in: $(pwd)"
    
    # 使用脚本所在目录作为CMake源目录
    cmake "$SCRIPT_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build .
    
    # 返回原目录
    cd "$CURRENT_DIR"   

    # 复制 Release 版本 - 放宽查找条件
    RELEASE_FILE=$(find "$BUILD_RELEASE_DIR" -maxdepth 1 -name "$PROJECT_NAME-*" -type f | head -1)
    if [ -n "$RELEASE_FILE" ] && [ -f "$RELEASE_FILE" ]; then
        cp "$RELEASE_FILE" "$OUTPUT_DIR/"
        echo "✓ Release version copied: $OUTPUT_DIR/$(basename "$RELEASE_FILE")"
    else
        echo "✗ Error: Release build failed - executable not found"
        echo "Looking for: $BUILD_RELEASE_DIR/$PROJECT_NAME-*"
        echo "Available files in $BUILD_RELEASE_DIR:"
        ls -la "$BUILD_RELEASE_DIR" 2>/dev/null || true
        exit 1
    fi
    
    RELEASE_END_TIME=$(date +%s)
    RELEASE_BUILD_TIME=$((RELEASE_END_TIME - RELEASE_START_TIME))
    release_hours=$((RELEASE_BUILD_TIME / 3600))
    release_min=$(( (RELEASE_BUILD_TIME % 3600) / 60 ))
    release_sec=$((RELEASE_BUILD_TIME % 60))
    
    if [ $release_hours -gt 0 ]; then
        echo "  Build time: ${release_hours} h ${release_min} min ${release_sec} sec"
    elif [ $release_min -gt 0 ]; then
        echo "  Build time: ${release_min} min ${release_sec} sec"
    else
        echo "  Build time: ${release_sec} sec"
    fi
fi

# 计算总时间
TOTAL_END_TIME=$(date +%s)
TOTAL_BUILD_TIME=$((TOTAL_END_TIME - TOTAL_START_TIME))
total_hours=$((TOTAL_BUILD_TIME / 3600))
total_min=$(( (TOTAL_BUILD_TIME % 3600) / 60 ))
total_sec=$((TOTAL_BUILD_TIME % 60))

echo ""
echo "=== ALL Completed Build ==="
echo "Generated files in $OUTPUT_DIR/:"
if [ -d "$OUTPUT_DIR" ]; then
    ls -lh "$OUTPUT_DIR" | grep -v cmake_ || true
else
    echo "Output directory not found!"
    exit 1
fi

echo ""
# echo "=== File Information ==="
# for file in "$OUTPUT_DIR"/"${PROJECT_NAME}"*; do
#     if [ -f "$file" ]; then
#         size_bytes=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
#         size_mb=$(echo "scale=2; $size_bytes / 1024 / 1024" | bc)
#         echo "File: $(basename "$file")"
#         echo "  Size: $(ls -lh "$file" | awk '{print $5}') (${size_mb} MB)"
#         echo "  Type: $(file "$file" 2>/dev/null | cut -d: -f2- || echo "Unknown")"
        
#         # 检测链接类型
#         echo -n "  Linking: "
#         if readelf -l "$file" 2>/dev/null | grep -q INTERP; then
#             echo "Dynamic"
#             echo "  Dynamic dependencies:"
#             ldd "$file" 2>/dev/null | while IFS= read -r line; do
#                 echo "    $line"
#             done || echo "    Cannot check dependencies"
#         else
#             echo "Static"
#             echo "  No dynamic dependencies"
#         fi
#         echo ""
#     fi
# done

# # 显示构建缓存目录状态
# echo "=== Build cache directories status ==="
# [ "$BUILD_DEBUG" = true ] && echo "Debug build dir: $(find "$BUILD_DEBUG_DIR" -type f 2>/dev/null | wc -l 2>/dev/null || echo 0) files" || echo "Debug build dir: Not built"
# [ "$BUILD_RELEASE" = true ] && echo "Release build dir: $(find "$BUILD_RELEASE_DIR" -type f 2>/dev/null | wc -l 2>/dev/null || echo 0) files" || echo "Release build dir: Not built"

# 显示总大小统计
for build_dir in "$OUTPUT_DIR"/cmake*; do
    if [ -d "$build_dir" ]; then
        for file in "$build_dir"/"$PROJECT_NAME"*; do
            if [ -f "$file" ]; then
                size_bytes=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
                size_mb=$(echo "scale=2; $size_bytes / 1024 / 1024" | bc)
                echo "File: $(basename "$file")"
                echo "  Size: $(ls -lh "$file" | awk '{print $5}') (${size_mb} MB)"
                echo "  Type: $(file "$file" 2>/dev/null | cut -d: -f2- || echo "Unknown")"
                
                # 检测链接类型
                echo -n "  Linking: "
                if readelf -l "$file" 2>/dev/null | grep -q INTERP; then
                    echo "Dynamic"
                    echo "  Dynamic dependencies:"
                    ldd "$file" 2>/dev/null | while IFS= read -r line; do
                        echo "    $line"
                    done || echo "    Cannot check dependencies"
                else
                    echo "Static"
                    echo "  No dynamic dependencies"
                fi
                echo ""
            fi
        done
    fi
done

    echo ""
    echo "=== Size Summary ==="
    total_bytes=0
    file_count=0
    for build_dir in "$OUTPUT_DIR"/cmake*; do
        if [ -d "$build_dir" ]; then
            for file in "$build_dir"/"$PROJECT_NAME"*; do
                if [ -f "$file" ]; then
                    size_bytes=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
                    total_bytes=$((total_bytes + size_bytes))
                    size_mb=$(echo "scale=2; $size_bytes / 1024 / 1024" | bc)
                    echo "  $(basename "$file"): ${size_mb} MB"
                    file_count=$((file_count + 1))
                fi
            done
        fi
    done

    if [ $file_count -gt 0 ]; then
        total_mb=$(echo "scale=2; $total_bytes / 1024 / 1024" | bc)
        echo "  Total: ${total_mb} MB"
    else
        echo "  No built files found"
    fi

# 显示构建时间统计
echo ""
echo "=== Build Time Statistics ==="
if [ "$BUILD_DEBUG" = true ]; then
    debug_hours=$((DEBUG_BUILD_TIME / 3600))
    debug_min=$(( (DEBUG_BUILD_TIME % 3600) / 60 ))
    debug_sec=$((DEBUG_BUILD_TIME % 60))
    if [ $debug_hours -gt 0 ]; then
        echo "  Debug version: ${debug_hours} h ${debug_min} min ${debug_sec} sec"
    elif [ $debug_min -gt 0 ]; then
        echo "  Debug version: ${debug_min} min ${debug_sec} sec"
    else
        echo "  Debug version: ${debug_sec} sec"
    fi
fi

if [ "$BUILD_RELEASE" = true ]; then
    release_hours=$((RELEASE_BUILD_TIME / 3600))
    release_min=$(( (RELEASE_BUILD_TIME % 3600) / 60 ))
    release_sec=$((RELEASE_BUILD_TIME % 60))
    if [ $release_hours -gt 0 ]; then
        echo "  Release version: ${release_hours} h ${release_min} min ${release_sec} sec"
    elif [ $release_min -gt 0 ]; then
        echo "  Release version: ${release_min} min ${release_sec} sec"
    else
        echo "  Release version: ${release_sec} sec"
    fi
fi

if [ $total_hours -gt 0 ]; then
    echo "  Total build time: ${total_hours} h ${total_min} min ${total_sec} sec"
elif [ $total_min -gt 0 ]; then
    echo "  Total build time: ${total_min} min ${total_sec} sec"
else
    echo "  Total build time: ${total_sec} sec"
fi

# # 显示构建速度比较（如果两个版本都构建了）
# if [ "$BUILD_DEBUG" = true ] && [ "$BUILD_RELEASE" = true ]; then
#     echo ""
#     echo "=== Build Speed Comparison ==="
#     if [ $DEBUG_BUILD_TIME -gt 0 ] && [ $RELEASE_BUILD_TIME -gt 0 ]; then
#         speed_ratio=$(echo "scale=2; $DEBUG_BUILD_TIME / $RELEASE_BUILD_TIME" | bc)
#         echo "  Release builds ${speed_ratio}x faster than Debug"
        
#         time_diff=$((DEBUG_BUILD_TIME - RELEASE_BUILD_TIME))
#         if [ $time_diff -gt 0 ]; then
#             diff_hours=$((time_diff / 3600))
#             diff_min=$(( (time_diff % 3600) / 60 ))
#             diff_sec=$((time_diff % 60))
#             if [ $diff_hours -gt 0 ]; then
#                 echo "  Debug took ${diff_hours} h ${diff_min} min ${diff_sec} sec longer than Release"
#             elif [ $diff_min -gt 0 ]; then
#                 echo "  Debug took ${diff_min} min ${diff_sec} sec longer than Release"
#             else
#                 echo "  Debug took ${diff_sec} sec longer than Release"
#             fi
#         else
#             abs_diff=$(( -time_diff ))
#             diff_hours=$((abs_diff / 3600))
#             diff_min=$(( (abs_diff % 3600) / 60 ))
#             diff_sec=$((abs_diff % 60))
#             if [ $diff_hours -gt 0 ]; then
#                 echo "  Release took ${diff_hours} h ${diff_min} min ${diff_sec} sec longer than Debug"
#             elif [ $diff_min -gt 0 ]; then
#                 echo "  Release took ${diff_min} min ${diff_sec} sec longer than Debug"
#             else
#                 echo "  Release took ${diff_sec} sec longer than Debug"
#             fi
#         fi
#     fi
# fi

echo ""
echo "=== Build finished successfully ==="