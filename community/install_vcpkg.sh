#!/bin/bash
set -euo pipefail  # 脚本执行出错时立即退出，避免隐患

##############################################################################
# 全局变量定义（统一管理，方便修改）
##############################################################################
VCPKG_DIR="/opt/vcpkg"
VCPKG_TRIPLETS_DIR="${VCPKG_DIR}/triplets/community"
BOOTSTRAP_SCRIPT="${VCPKG_DIR}/bootstrap-vcpkg.sh"
# 环境变量配置内容（作为指纹，用于去重检查）
ENV_CONFIG=$(cat <<EOF
# vcpkg
export VCPKG_ROOT=${VCPKG_DIR}
export PATH=\$VCPKG_ROOT:\$PATH
EOF
)

##############################################################################
# 辅助函数：检测系统发行版（核心适配Alpine）
##############################################################################
detect_distro() {
    if [ -f "/etc/alpine-release" ]; then
        echo "alpine"
    elif [ -f "/etc/debian_version" ]; then
        echo "debian"
    elif [ -f "/etc/redhat-release" ] || [ -f "/etc/centos-release" ]; then
        echo "rhel"
    else
        echo "unknown"
    fi
}

##############################################################################
# 前置检查：根据发行版安装依赖工具（适配Alpine/apk、Debian/apt、RHEL/dnf）
##############################################################################
check_dependencies() {
    local distro=$(detect_distro)
    local dependencies=()
    local install_cmd=""

    # 按发行版定义依赖和安装命令
    case "$distro" in
        "alpine")
            # Alpine必需依赖：bash（脚本依赖）、git、cmake、gcc、g++、make、musl-dev（编译依赖）、curl（bootstrap可能需要）
            dependencies=("bash" "git" "cmake" "gcc" "g++" "make" "musl-dev" "curl" "sudo")
            install_cmd="sudo apk add --no-cache"
            ;;
        "debian")
            dependencies=("git" "cmake" "gcc" "g++" "sudo")
            install_cmd="sudo apt update && sudo apt install -y"
            ;;
        "rhel")
            dependencies=("git" "cmake" "gcc" "g++" "sudo")
            install_cmd="sudo dnf install -y"
            ;;
        "unknown")
            echo "错误：不支持的Linux发行版！"
            exit 1
            ;;
    esac

    # 检查并安装缺失的依赖
    local missing_deps=()
    for dep in "${dependencies[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing_deps+=("$dep")
        fi
    done

    if [ ${#missing_deps[@]} -gt 0 ]; then
        echo "🔧 缺少必需工具：${missing_deps[*]}，正在自动安装..."
        eval "$install_cmd ${missing_deps[*]}"
    fi

    echo "✅ 所有依赖工具已安装（发行版：$distro）"
}

##############################################################################
# 步骤1：克隆vcpkg到/opt目录（已存在则询问是否删除重装）
##############################################################################
clone_vcpkg() {
    # 检查目录是否已存在
    if [ -d "$VCPKG_DIR" ]; then
        echo -e "\n⚠️  检测到 ${VCPKG_DIR} 目录已存在！"
        read -p "是否删除现有目录并全新安装？[y/N] " choice
        case "$choice" in
            [yY])
                echo "🔧 正在删除现有 ${VCPKG_DIR} 目录..."
                sudo rm -rf "$VCPKG_DIR"
                echo "✅ 现有目录删除完成"
                ;;
            *)
                echo "ℹ️  选择保留现有目录，跳过克隆步骤"
                return
                ;;
        esac
    fi

    # 克隆仓库到/opt目录（需要sudo权限）
    echo "🔧 正在克隆vcpkg到 ${VCPKG_DIR} 目录..."
    sudo git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
    # 设置全权限（满足普通用户和root都能读写）
    echo "🔧 正在设置 ${VCPKG_DIR} 目录权限..."
    sudo chmod 777 -R "$VCPKG_DIR"
    echo "✅ vcpkg克隆及权限设置完成"
}

##############################################################################
# 步骤2：拷贝脚本所在目录的*.cmake到triplets/community（修复目录问题）
##############################################################################
copy_cmake_files() {
    # 获取脚本所在目录的绝对路径（核心修复：不管在哪里执行脚本，都以脚本所在目录为准）
    SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
    echo "ℹ️  正在读取脚本所在目录（${SCRIPT_DIR}）的*.cmake文件"
    
    # 检查脚本目录是否有*.cmake文件
    shopt -s nullglob  # 避免无匹配文件时出现"*.cmake"字面量
    local cmake_files=("${SCRIPT_DIR}"/*.cmake)
    shopt -u nullglob
    
    if [ ${#cmake_files[@]} -eq 0 ]; then
        echo "⚠️  脚本所在目录（${SCRIPT_DIR}）未找到任何*.cmake文件，跳过拷贝步骤"
        return
    fi
    
    # 确保目标目录存在（如果不存在则创建）
    if [ ! -d "$VCPKG_TRIPLETS_DIR" ]; then
        echo "🔧 创建目标目录 ${VCPKG_TRIPLETS_DIR}..."
        mkdir -p "$VCPKG_TRIPLETS_DIR"
        sudo chmod 777 -R "${VCPKG_DIR}/triplets"  # 确保子目录也有写权限
    fi
    
    # 拷贝文件（普通用户权限即可，因目录已设777）
    echo "🔧 正在拷贝 ${#cmake_files[@]} 个*.cmake文件到 ${VCPKG_TRIPLETS_DIR}..."
    cp -v "${SCRIPT_DIR}"/*.cmake "$VCPKG_TRIPLETS_DIR/"
    echo "✅ *.cmake文件拷贝完成"
}

##############################################################################
# 步骤3：执行bootstrap-vcpkg.sh脚本（适配Alpine编译环境）
##############################################################################
bootstrap_vcpkg() {
    local distro=$(detect_distro)
    # 检查bootstrap脚本是否存在（避免目录存在但文件缺失的情况）
    if [ ! -f "$BOOTSTRAP_SCRIPT" ]; then
        echo "错误：未找到 ${BOOTSTRAP_SCRIPT} 脚本，请检查vcpkg目录是否完整！"
        exit 1
    fi
    
    # Alpine特殊处理：设置CC/CXX环境变量（确保使用gcc而非默认的musl-gcc）
    if [ "$distro" = "alpine" ]; then
        echo "🔧 Alpine环境特殊配置：设置CC/CXX编译器..."
        export CC=gcc
        export CXX=g++
    fi
    
    echo "🔧 正在初始化vcpkg（执行bootstrap脚本）..."
    cd "$VCPKG_DIR" || exit 1  # 进入vcpkg目录执行
    bash "$BOOTSTRAP_SCRIPT"  # 普通用户即可执行（目录有777权限）
    cd - > /dev/null  # 回到原目录
    echo "✅ vcpkg初始化完成"
}

##############################################################################
# 步骤4：配置环境变量（去重检查，避免重复添加）
##############################################################################
configure_environment() {
    local distro=$(detect_distro)
    # 检查并添加普通用户的.bashrc（Alpine默认bash的配置文件也是.bashrc）
    local user_bashrc="$HOME/.bashrc"
    # 使用md5sum判断配置块是否已存在（避免部分匹配）
    local config_md5=$(echo -n "$ENV_CONFIG" | md5sum | awk '{print $1}')
    local existing_md5=$(grep -A2 "^# vcpkg" "$user_bashrc" 2>/dev/null | md5sum | awk '{print $1}')
    
    if [ "$config_md5" != "$existing_md5" ]; then
        echo "🔧 正在配置普通用户环境变量（$user_bashrc）..."
        echo -e "\n$ENV_CONFIG" >> "$user_bashrc"  # 加换行避免和其他内容粘连
        source "$user_bashrc"  # 立即生效
    else
        echo "⚠️  普通用户.bashrc已存在vcpkg配置，跳过添加"
    fi
    
    # 检查并添加root用户的.bashrc（需要sudo权限）
    local root_bashrc="/root/.bashrc"
    local root_existing_md5=$(sudo grep -A2 "^# vcpkg" "$root_bashrc" 2>/dev/null | md5sum | awk '{print $1}')
    
    if [ "$config_md5" != "$root_existing_md5" ]; then
        echo "🔧 正在配置root用户环境变量（$root_bashrc）..."
        echo -e "\n$ENV_CONFIG" | sudo tee -a "$root_bashrc" > /dev/null
        sudo bash -c "source $root_bashrc"  # 让root环境立即生效
    else
        echo "⚠️  root用户.bashrc已存在vcpkg配置，跳过添加"
    fi
    
    # Alpine额外配置：如果用户使用ash（默认shell），可选择添加到~/.profile（可选）
    if [ "$distro" = "alpine" ] && [ -f "$HOME/.profile" ] && ! grep -q "VCPKG_ROOT" "$HOME/.profile"; then
        read -p "⚠️  检测到Alpine默认shell（ash），是否将vcpkg配置添加到~/.profile？[y/N] " choice
        case "$choice" in
            [yY])
                echo -e "\n$ENV_CONFIG" >> "$HOME/.profile"
                source "$HOME/.profile"
                echo "✅ 已添加vcpkg配置到~/.profile（适配ash shell）"
                ;;
            *)
                echo "ℹ️  跳过~/.profile配置，仅保留.bashrc配置（使用bash时生效）"
                ;;
        esac
    fi
    
    echo "✅ 环境变量配置完成"
}

##############################################################################
# 步骤5：验证权限和安装结果
##############################################################################
verify_installation() {
    echo -e "\n🔍 正在验证安装结果..."
    # 验证vcpkg命令可执行
    if command -v vcpkg &> /dev/null; then
        echo "✅ vcpkg命令可正常调用（版本：$(vcpkg --version | head -n1)）"
    else
        echo "警告：vcpkg命令未找到，请手动执行 source ~/.bashrc 后重试"
        if [ "$(detect_distro)" = "alpine" ]; then
            echo "      若使用ash shell，请执行 source ~/.profile"
        fi
    fi
    
    # 验证权限（普通用户和root都能读写）
    local test_file="${VCPKG_DIR}/test_permission.txt"
    touch "$test_file" &> /dev/null && rm -f "$test_file" || {
        echo "错误：普通用户无${VCPKG_DIR}写入权限！"
        exit 1
    }
    sudo touch "$test_file" &> /dev/null && sudo rm -f "$test_file" || {
        echo "错误：root用户无${VCPKG_DIR}写入权限！"
        exit 1
    }
    echo "✅ 普通用户和root用户均拥有${VCPKG_DIR}读写权限"
}

##############################################################################
# 主执行流程
##############################################################################
main() {
    echo "========================================"
    echo "        Linux vcpkg 安装脚本（Alpine兼容版）"
    echo "========================================"
    check_dependencies
    clone_vcpkg
    copy_cmake_files
    bootstrap_vcpkg
    configure_environment
    verify_installation
    echo "========================================"
    echo "🎉 vcpkg 安装配置全部完成！"
    echo "📌 验证方法：打开新终端执行 vcpkg --version"
    echo "📌 VCPKG_ROOT: ${VCPKG_DIR}"
    echo "📌 CMake文件来源：脚本所在目录（${SCRIPT_DIR}）"
    if [ "$(detect_distro)" = "alpine" ]; then
        echo "📌 提示：Alpine默认使用ash shell，若未生效请执行 source ~/.profile"
    fi
    echo "========================================"
}

# 启动主流程
main