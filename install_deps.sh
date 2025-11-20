#!/bin/bash

# 检测系统架构
ARCH=$(uname -m)

# 根据架构设置对应的triplet
case "$ARCH" in
    x86_64)
        TRIPLET="microear-x64-linux-dynamic"
        ;;
    arm64|aarch64)
        # 兼容arm64和aarch64两种输出（部分Linux系统arm64架构显示为aarch64）
        TRIPLET="microear-arm64-linux-dynamic"
        ;;
    *)
        echo "错误：不支持的系统架构 '$ARCH'，仅支持x86_64和arm64/aarch64"
        exit 1
        ;;
esac

# 导出环境变量
export VCPKG_TARGET_TRIPLET="$TRIPLET"
export VCPKG_HOST_TRIPLET="$TRIPLET"
export VCPKG_ALLOW_MIXED_TRIPLETS=0

# 执行vcpkg安装
vcpkg install \
    --triplet="$VCPKG_TARGET_TRIPLET" \
    --host-triplet="$VCPKG_HOST_TRIPLET" \
    --x-install-root="$(pwd)/vcpkg_installed"

# 输出安装信息（可选，方便调试）
echo "----------------------------------------"
echo "架构检测结果：$ARCH"
echo "使用的triplet：$TRIPLET"
echo "安装目录：$(pwd)/vcpkg_installed"
echo "----------------------------------------"