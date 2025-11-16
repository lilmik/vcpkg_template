#!/bin/bash
set -euo pipefail

# ========================= 配置区（适配 vcpkg Preset 模式）=========================
# 1. 必须保留的核心目录/文件（重点：保留 build 目录及 vcpkg Preset 关键内容）
KEEP_DIRS=(
    "vcpkg_installed"               # 全局依赖库目录（核心保留）
    "install"                       # 最终安装目录（bin/lib 所在）
    "build"                         # 保留 build 目录本身（vcpkg Preset 依赖缓存在此）
    "src"                           # 源码目录
    "include"                       # 头文件目录
    "Dockerfile.scratch.universal"  # Docker 配置
    "build-image.sh"                # 构建脚本
    "run-image.sh"                  # 运行脚本
    "ld-list.sh"                    # 依赖查看脚本
    "manifest.txt"                  # 清单文件
    "CMakeLists.txt"                # 项目构建配置
    "CMakePresets.json"             # vcpkg Preset 配置文件（核心保留）
    "vcpkg.json"                    # vcpkg 依赖配置（核心保留）
    "helloworld.h"                  # 自定义头文件
)

# 2. 要清理的目录（非 build 目录，或 build 内的临时子目录）
CLEAN_DIRS=(
    "tmp"                           # 全局临时目录
    ".cache"                        # 全局缓存目录
    "vcpkg_build"                   # vcpkg 全局构建临时目录（非 installed）
    "vcpkg/packages"                # vcpkg 全局包缓存（可清理）
    "vcpkg/buildtrees"              # vcpkg 全局构建树（可清理）
    "vcpkg/downloads"               # vcpkg 全局下载缓存（可清理，重构建会重下）
    "vcpkg/ports"                   # vcpkg 全局端口文件（可清理，重构建会恢复）
)

# 3. 要清理的文件（全局 + build 目录内的临时文件，支持通配符递归匹配）
# 重点：用 `**/` 匹配 build 目录下所有子目录中的临时文件，不删目录本身
CLEAN_FILES=(
    # 全局临时文件
    "CMakeCache.txt"                # 根目录 CMake 缓存（若有）
    "cmake_install.cmake"           # 根目录安装配置（若有）
    "Makefile"                      # 根目录 Makefile（若有）
    "*.make"                        # 所有 .make 后缀文件
    "*.o"                           # 所有目标文件
    "*.so.tmp"                      # 临时动态库（非最终产物）
    "*.a.tmp"                       # 临时静态库（非最终产物）
    "*.log"                         # 所有日志文件
    "*.tmp"                         # 所有临时文件
    ".cmake"                        # 根目录 CMake 临时目录
    "compile_commands.json"         # 编译命令日志（如需保留可删除此行）

    # build 目录内的临时文件（递归匹配所有子目录）
    "build/**/CMakeCache.txt"       # build 下所有 CMake 缓存
    "build/**/CMakeFiles"           # build 下所有 CMake 中间目录
    "build/**/*.o"                  # build 下所有目标文件
    "build/**/*.log"                # build 下所有日志文件
    "build/**/*.tmp"                # build 下所有临时文件
    "build/**/*.d"                  # 编译依赖文件（可重建）
    "build/**/.cmake"               # build 下所有 CMake 临时目录
    "build/**/cmake_install.cmake"  # build 下所有安装配置文件
    "build/**/Makefile"             # build 下所有生成的 Makefile
    "build/**/*.make"               # build 下所有 .make 后缀文件
)
# =============================================================================

# 安全检查：确保在项目根目录执行（避免误删其他目录文件）
REQUIRED_FILES=("CMakeLists.txt" "CMakePresets.json" "vcpkg_installed")
found=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ] || [ -d "$file" ]; then
        found=$((found + 1))
    fi
done
if [ $found -lt 2 ]; then
    echo "❌ 错误：未在 vcpkg Preset 项目根目录执行！"
    echo "   请切换到包含 CMakeLists.txt、CMakePresets.json 和 vcpkg_installed 的目录后运行"
    exit 1
fi

# 函数：预览要删除的内容（递归显示，不实际删除）
dry_run() {
    echo -e "\n📋 预览模式：以下内容将被清理（不会实际删除）"
    echo "------------------------------------------------"

    # 预览要清理的目录
    for dir in "${CLEAN_DIRS[@]}"; do
        if [ -d "$dir" ] || compgen -G "$dir" > /dev/null; then
            echo " Directory: $dir"
        fi
    done

    # 预览要清理的文件（支持递归匹配 build 子目录）
    for pattern in "${CLEAN_FILES[@]}"; do
        # 用 find 命令递归查找匹配的文件/目录，避免通配符失效
        matches=$(find . -path "$pattern" 2>/dev/null)
        if [ -n "$matches" ]; then
            echo "    Pattern: $pattern"
            echo "      Matches:"
            echo "$matches" | sed 's/^/        /'
        fi
    done

    echo "------------------------------------------------"
    echo "✅ 以下内容将被保留（核心目录/文件）"
    for keep in "${KEEP_DIRS[@]}"; do
        if [ -f "$keep" ] || [ -d "$keep" ]; then
            echo "    $keep"
        fi
    done
    echo "ℹ️  提示：build 目录内的依赖缓存（_deps/、vcpkg_installed/）、生成的库/程序均会保留"
}

# 函数：执行实际清理（递归清理，保留目录结构）
do_clean() {
    echo -e "\n🗑️  开始清理临时文件（保留 build 目录核心内容）..."
    echo "------------------------------------------------"

    # 第一步：清理全局临时目录
    for dir in "${CLEAN_DIRS[@]}"; do
        for actual_dir in $(compgen -G "$dir" || true); do
            if [[ " ${KEEP_DIRS[@]} " =~ " ${actual_dir} " ]]; then
                echo "⚠️  跳过保留目录：$actual_dir"
                continue
            fi
            if [ -d "$actual_dir" ]; then
                echo "删除目录：$actual_dir"
                rm -rf "$actual_dir"
            fi
        done
    done

    # 第二步：递归清理临时文件（全局 + build 子目录）
    for pattern in "${CLEAN_FILES[@]}"; do
        matches=$(find . -path "$pattern" 2>/dev/null)
        if [ -n "$matches" ]; then
            echo "处理模式：$pattern"
            while IFS= read -r target; do
                # 跳过保留列表中的文件/目录（双重保险）
                if [[ " ${KEEP_DIRS[@]} " =~ " $(basename "$target") " ]]; then
                    echo "  ⚠️  跳过保留：$target"
                    continue
                fi
                # 删除文件或目录
                if [ -f "$target" ]; then
                    echo "  删除文件：$target"
                    rm -f "$target"
                elif [ -d "$target" ]; then
                    echo "  删除目录：$target"
                    rm -rf "$target"
                fi
            done <<< "$matches"
        fi
    done

    echo "------------------------------------------------"
    echo "✅ 清理完成！"
    echo "ℹ️  保留内容："
    echo "   - vcpkg_installed（全局依赖库）"
    echo "   - build 目录及其中的依赖缓存（_deps/）、Preset 配置、生成的库/程序"
    echo "   - install 目录（最终安装产物）、源码、Docker 配置等"
    echo "ℹ️  下次构建时，vcpkg Preset 会复用已有依赖，新增库将增量追加，无需重新拷贝"
}

# 主逻辑：解析参数
if [ $# -eq 1 ] && [ "$1" == "--dry-run" ]; then
    dry_run
    exit 0
fi

# 确认清理（避免误操作）
echo "⚠️  警告：此脚本仅清理临时文件，保留 build 目录核心内容（依赖缓存/生成产物）"
read -p "是否继续清理？[y/N] " -n 1 -r
echo -e "\n"
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "🚫 已取消清理"
    exit 0
fi

# 执行清理
do_clean