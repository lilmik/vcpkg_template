#!/usr/bin/env bash
# build-microear.sh
# 自动根据主机平台编译相应的 {dynamic|static} × {debug|release} 组合。
# 预设名形如：microear-<x64|arm64>-linux-<dynamic|static>-<debug|release>
# 任何目录执行本脚本都有效：会先 cd 到脚本所在的项目根目录。

set -euo pipefail

# ---------- 稳健定位项目根目录 ----------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd || echo /)"
cd "$ROOT" || cd /

# ---------- 配置 ----------
# 默认：根据主机自动识别架构；构建 dynamic+static × debug+release 全矩阵
ARCH_AUTO="$(uname -m | tr '[:upper:]' '[:lower:]')"
case "$ARCH_AUTO" in
  x86_64|amd64) ARCH_DEFAULT="x64" ;;
  aarch64|arm64) ARCH_DEFAULT="arm64" ;;
  *) ARCH_DEFAULT="x64" ;;  # 不识别时默认 x64，可用 --arch 覆盖
esac

ARCH="${ARCH:-$ARCH_DEFAULT}"        # 可用 env 覆盖：ARCH=x64/arm64
LINK_KIND="${LINK_KIND:-all}"         # env：dynamic|static|all
CONFIG_KIND="${CONFIG_KIND:-all}"     # env：debug|release|all
DRY_RUN="${DRY_RUN:-0}"               # env：1=只打印不执行
JOBS="${JOBS:-}"                      # env：并行度；空则自动探测

# ---------- 参数解析 ----------
usage() {
  cat <<EOF
Usage:
  $0 [--arch x64|arm64] [--link dynamic|static|all] [--config debug|release|all] [--dry-run] [-j N]
  例：
    $0                               # 当前平台的 dynamic+static × debug+release 全部构建
    $0 --link dynamic --config debug # 只构建 dynamic+debug
    $0 --arch arm64 --link static    # ARM64 的 static × (debug+release)
    JOBS=8 $0 --link dynamic         # 指定并行度 8
    DRY_RUN=1 $0 --link static       # 仅打印将执行的命令
EOF
}

to_lower(){ echo "$1" | tr '[:upper:]' '[:lower:]'; }

ARGS=("$@")
i=0
while (( i < ${#ARGS[@]} )); do
  case "${ARGS[i]}" in
    --arch)     ARCH="$(to_lower "${ARGS[i+1]:-}")"; i=$((i+2));;
    --link)     LINK_KIND="$(to_lower "${ARGS[i+1]:-}")"; i=$((i+2));;
    --config)   CONFIG_KIND="$(to_lower "${ARGS[i+1]:-}")"; i=$((i+2));;
    --dry-run)  DRY_RUN=1; i=$((i+1));;
    -j|--jobs)  JOBS="${ARGS[i+1]:-}"; i=$((i+2));;
    -h|--help)  usage; exit 0;;
    *) echo "[WARN] unknown arg: ${ARGS[i]}"; i=$((i+1));;
  esac
done

# ---------- 归一化 ----------
case "$(to_lower "$ARCH")" in
  x64|amd64|x86_64) ARCH="x64" ;;
  arm64|aarch64)    ARCH="arm64" ;;
  *) echo "[ERR] unsupported arch: $ARCH"; exit 1;;
esac

case "$LINK_KIND" in
  dynamic|static|all) ;;
  *) echo "[ERR] --link expects dynamic|static|all"; exit 1;;
esac

case "$CONFIG_KIND" in
  debug|release|all) ;;
  *) echo "[ERR] --config expects debug|release|all"; exit 1;;
esac

# ---------- 组合矩阵 ----------
LINKS=()
CONFIGS=()

if [[ "$LINK_KIND" == "all" ]]; then
  LINKS=(dynamic static)
else
  LINKS=("$LINK_KIND")
fi

if [[ "$CONFIG_KIND" == "all" ]]; then
  CONFIGS=(debug release)
else
  CONFIGS=("$CONFIG_KIND")
fi

# ---------- 并行度 ----------
if [[ -z "$JOBS" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  else
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  fi
fi

# ---------- 工具检测 ----------
need() { command -v "$1" >/dev/null 2>&1 || { echo "[ERR] missing command: $1"; exit 1; }; }
need cmake

# ---------- 执行函数 ----------
run() {
  echo "+ $*"
  if [[ "$DRY_RUN" != "1" ]]; then
    eval "$@"
  fi
}

# ---------- 主流程 ----------
echo "[INFO] Project root: $ROOT"
echo "[INFO] Arch: $ARCH"
echo "[INFO] Matrix: LINKS=${LINKS[*]}  CONFIGS=${CONFIGS[*]}"
echo "[INFO] Jobs: $JOBS"
echo

TOTAL=0
BUILT=0
FAILED=()

for link in "${LINKS[@]}"; do
  for cfg in "${CONFIGS[@]}"; do
    PRESET="microear-${ARCH}-linux-${link}-${cfg}"
    TOTAL=$((TOTAL+1))
    echo "==== Building preset: ${PRESET} ===="

    # 先配置
    if ! run "cmake --preset=${PRESET}"; then
      echo "[WARN] configure failed for ${PRESET}"
      FAILED+=("${PRESET} (configure)")
      echo
      continue
    fi

    # 再编译
    if run "cmake --build --preset=${PRESET} --parallel ${JOBS}"; then
      BUILT=$((BUILT+1))
    else
      echo "[WARN] build failed for ${PRESET}"
      FAILED+=("${PRESET} (build)")
    fi
    echo
  done
done

echo "===== SUMMARY ====="
echo "Total: ${TOTAL}, Success: ${BUILT}, Failed: ${#FAILED[@]}"
if ((${#FAILED[@]})); then
  printf ' - %s\n' "${FAILED[@]}"
fi

# 可选：编译后自动打包（集成你前面用的 bundle 脚本）
# 若希望自动打包，取消下面注释即可：
# if [[ "$DRY_RUN" != "1" && -x "$ROOT/bundle-linux-auto.sh" ]]; then
#   echo
#   echo ">>> Running bundler (bundle-linux-auto.sh) ..."
#   "$ROOT/bundle-linux-auto.sh"
# fi
