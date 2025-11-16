# 脚本内容（复制粘贴进去）
#!/bin/bash
export VCPKG_TARGET_TRIPLET=microear-x64-linux-dynamic
export VCPKG_HOST_TRIPLET=microear-x64-linux-dynamic
export VCPKG_ALLOW_MIXED_TRIPLETS=0
vcpkg install --triplet=$VCPKG_TARGET_TRIPLET --host-triplet=$VCPKG_HOST_TRIPLET --dry-run