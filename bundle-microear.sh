#!/usr/bin/env bash
# bundle-microear.sh
# 自动扫描 ./build/microear-*-linux-* ，区分静态/动态，把可执行和依赖打到 ./install/<preset>/ 里，
# 并自动生成 Dockerfile + 构建/运行/检查脚本。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# 默认可执行名字（你现在都是 HelloWorld，可以用 EXE_NAME=MyApp 覆盖）
EXE_NAME="${EXE_NAME:-HelloWorld}"

log(){ echo "[INFO] $*"; }
warn(){ echo "[WARN] $*" >&2; }
err(){ echo "[ERR]  $*" >&2; }
need(){ command -v "$1" >/dev/null 2>&1 || { err "missing command: $1"; exit 1; }; }

# 必要工具
need readelf
need find
need awk
need sed
command -v file >/dev/null 2>&1 || warn "command 'file' not found, arch fallback will be limited"

# =========================
# 1. 宿主架构识别
# =========================
HOST_RAW="$(uname -m | tr '[:upper:]' '[:lower:]')"
case "$HOST_RAW" in
  x86_64|amd64) HOST_ARCH="x86_64" ;;
  aarch64|arm64) HOST_ARCH="aarch64" ;;
  *) HOST_ARCH="$HOST_RAW" ;;
esac
log "Host arch: $HOST_ARCH"

# =========================
# 2. ELF 架构识别（更鲁棒）
# =========================
elf_arch() {
  local f="$1"
  local m
  m="$(readelf -h "$f" 2>/dev/null | awk -F: '/Machine:/ {gsub(/^[ \t]+/, "", $2); print $2}' | tr '[:upper:]' '[:lower:]' || true)"
  if [[ "$m" == *x86-64* ]]; then echo x86_64; return 0; fi
  if [[ "$m" == *aarch64* || "$m" == *arm64* ]]; then echo aarch64; return 0; fi
  # fallback 到 file
  if command -v file >/dev/null 2>&1; then
    local fi
    fi="$(file -b "$f" 2>/dev/null | tr '[:upper:]' '[:lower:]' || true)"
    if [[ "$fi" == *x86-64* ]]; then echo x86_64; return 0; fi
    if [[ "$fi" == *aarch64* || "$fi" == *arm64* ]]; then echo aarch64; return 0; fi
  fi
  echo unknown
}

# =========================
# 3. 判断静态/动态
# =========================
is_dynamic() {
  readelf -l "$1" | grep -q 'Requesting program interpreter'
}

# =========================
# 4. 解析 ldd （动态依赖）
# =========================
parse_ldd_paths() {
  local f="$1" out
  out="$(ldd "$f" 2>/dev/null || true)"
  if grep -q "=> not found" <<<"$out"; then
    err "missing dependency for $f:"
    grep "=> not found" <<<"$out" >&2
    exit 1
  fi
  # 把真正的绝对路径提出来
  awk '
    /=>/ { if ($3 != "not") print $3; next }
    /^[[:space:]]*\// { print $1 }
  ' <<<"$out" \
  | sed 's/[[:space:]]*$//' \
  | sed 's/[[:space:]]*(.*$//'
}

# =========================
# 5. 拷贝库 + 重建软链
# =========================
copy_lib_with_symlink() {
  local src="$1" dst="$2"
  [ -e "$src" ] || { err "lib not exist: $src"; return 1; }
  local real base_src base_real
  real="$(readlink -f "$src")" || { err "readlink -f failed: $src"; return 1; }
  base_src="$(basename "$src")"
  base_real="$(basename "$real")"

  # 先拷实体
  if [ ! -e "$dst/$base_real" ]; then
    cp -a "$real" "$dst/$base_real"
    log "copied $real -> $dst/$base_real"
  fi

  # 如果原本是软链，重建软链名
  if [ "$base_src" != "$base_real" ]; then
    ln -sfn "$base_real" "$dst/$base_src"
    log "symlink $dst/$base_src -> $base_real"
  fi
}

# =========================
# 6. 递归闭包收集依赖
# =========================
collect_deps_closure() {
  local root="$1"; declare -gA SEEN=()
  local -a q=("$root")
  local EXCLUDE='linux-vdso\.so'
  while ((${#q[@]})); do
    local it="${q[0]}"; q=("${q[@]:1}")
    local dep
    while IFS= read -r dep; do
      [ -z "$dep" ] && continue
      [[ "$dep" =~ $EXCLUDE ]] && continue
      [[ "$dep" = /* ]] || continue
      if [[ -z "${SEEN[$dep]+x}" ]]; then
        SEEN["$dep"]=1
        q+=("$dep")
      fi
    done < <(parse_ldd_paths "$it" || true)
  done
}

# =========================
# 7. 生成各自目录下的 build/run/ld-list
# =========================
write_helper_scripts(){
  local install_root="$1" preset="$2" dockerfile="$3" kind="$4" interp_base="${5:-}"

  # build-image.sh —— 单架构构建
  cat > "$install_root/build-image.sh" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
# 稳健切到脚本目录（WSL/挂载路径异常时兜底到 /）
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd || echo /)"
cd "$DIR" || cd /
IMAGE="${IMAGE:-__IMAGE__}"
DOCKERFILE="${DOCKERFILE:-__DOCKERFILE__}"
docker build -f "$DOCKERFILE" -t "$IMAGE" .
echo "Built image: $IMAGE"
EOS
  sed -i "s#__IMAGE__#$preset#g" "$install_root/build-image.sh"
  sed -i "s#__DOCKERFILE__#$(basename "$dockerfile")#g" "$install_root/build-image.sh"
  chmod +x "$install_root/build-image.sh"

  # run-image.sh —— 直接运行镜像（同样先切到脚本目录，避免 shell-init 报错）
  cat > "$install_root/run-image.sh" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd || echo /)"
cd "$DIR" || cd /
IMAGE="${IMAGE:-__IMAGE__}"
# 透传额外参数到容器
exec docker run --rm "$IMAGE" "$@"
EOS
  sed -i "s#__IMAGE__#$preset#g" "$install_root/run-image.sh"
  chmod +x "$install_root/run-image.sh"

  # ld-list.sh —— 仅动态产物生成，用动态链接器列出容器内加载库
  if [[ "$kind" == "dynamic" ]]; then
    cat > "$install_root/ld-list.sh" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd || echo /)"
cd "$DIR" || cd /
IMAGE="${IMAGE:-__IMAGE__}"
INTERP="__INTERP_BASENAME__"
exec docker run --rm --entrypoint "/app/lib/${INTERP}" "$IMAGE" --list /app/bin/_entry
EOS
    sed -i "s#__IMAGE__#$preset#g" "$install_root/ld-list.sh"
    sed -i "s#__INTERP_BASENAME__#$interp_base#g" "$install_root/ld-list.sh"
    chmod +x "$install_root/ld-list.sh"
  fi
}

# =========================
# 8. 打包动态
# =========================
bundle_dynamic() {
  local preset_dir="$1" preset="$2" exec_path="$3"
  need ldd
  need patchelf

  local install_root="$ROOT/install/$preset"
  local bin_dir="$install_root/bin"
  local lib_dir="$install_root/lib"
  mkdir -p "$bin_dir" "$lib_dir"

  install -m 0755 "$exec_path" "$bin_dir/$EXE_NAME"
  local exec_dst="$bin_dir/$EXE_NAME"

  # 收集依赖
  collect_deps_closure "$exec_path"

  # 解释器
  local interp
  interp="$(readelf -l "$exec_path" | awk '/Requesting program interpreter/ {print $NF}' | tr -d '[]')"
  [ -n "$interp" ] && [ -e "$interp" ] || { err "cannot get PT_INTERP for $exec_path"; exit 1; }
  SEEN["$interp"]=1

  log "[$preset] deps: ${#SEEN[@]} (including loader)"

  # 全部拷进 lib
  for dep in "${!SEEN[@]}"; do
    [[ "$dep" == "$lib_dir/"* ]] && continue
    copy_lib_with_symlink "$dep" "$lib_dir" || warn "copy failed: $dep"
  done

  # 确保解释器是实体
  local interp_base; interp_base="$(basename "$interp")"
  if [ -L "$lib_dir/$interp_base" ]; then
    rm -f "$lib_dir/$interp_base"
    cp --dereference "$interp" "$lib_dir/$interp_base"
    log "[$preset] loader replaced as entity: $lib_dir/$interp_base"
  fi

  # 写回可执行
  patchelf --set-interpreter "$interp" "$exec_dst"
  patchelf --force-rpath --set-rpath '$ORIGIN/../lib' "$exec_dst"

  # 给 lib/*.so* 写 RPATH=$ORIGIN
  while IFS= read -r so; do
    local base; base="$(basename "$so")"
    if [[ "$base" == ld-linux* || "$base" == ld-*.so* ]]; then
      continue
    fi
    patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null && \
      log "[$preset] set RPATH(\$ORIGIN) -> $base" || true
  done < <(find "$lib_dir" -maxdepth 1 -type f -name '*.so*' -print)

  # 生成 manifest
  {
    echo "# Executable (dynamic)"; echo "$exec_dst"; echo
    echo "# Interpreter (absolute)"; echo "$interp"; echo
    echo "# Libraries (original paths)"; for dep in "${!SEEN[@]}"; do echo "$dep"; done | sort
  } > "$install_root/manifest.txt"

  # 生成 Dockerfile
  local dockerfile="$install_root/Dockerfile.scratch.universal"
  cat > "$dockerfile" <<EOF
FROM scratch
COPY lib/$interp_base $interp
COPY lib /app/lib
COPY bin /app/bin
COPY bin/$EXE_NAME /app/bin/_entry
ENTRYPOINT ["/app/bin/_entry"]
EOF

  # 生成构建/运行/检查脚本
  write_helper_scripts "$install_root" "$preset" "$dockerfile" "dynamic" "$interp_base"

  log "[$preset] dynamic bundle done -> $install_root"
}

# =========================
# 9. 打包静态
# =========================
bundle_static() {
  local preset_dir="$1" preset="$2" exec_path="$3"
  local install_root="$ROOT/install/$preset"
  local bin_dir="$install_root/bin"
  mkdir -p "$bin_dir"

  install -m 0755 "$exec_path" "$bin_dir/$EXE_NAME"

  {
    echo "# Executable (static)"; echo "$bin_dir/$EXE_NAME"
  } > "$install_root/manifest.txt"

  local dockerfile="$install_root/Dockerfile.scratch.static"
  cat > "$dockerfile" <<EOF
FROM scratch
COPY bin /app/bin
ENTRYPOINT ["/app/bin/$EXE_NAME"]
EOF

  write_helper_scripts "$install_root" "$preset" "$dockerfile" "static"

  log "[$preset] static bundle done -> $install_root"
}

# =========================
# 10. 主循环：扫描 build/
# =========================
FOUND=0
PROCESSED=0
SKIPPED=()

while IFS= read -r preset_dir; do
  preset="$(basename "$preset_dir")"

  # 找可执行
  exec_path="$preset_dir/$EXE_NAME"
  if [ ! -x "$exec_path" ]; then
    # 尝试探测一个可执行
    probe="$(find "$preset_dir" -maxdepth 1 -type f -executable -print | head -n1 || true)"
    if [ -n "$probe" ]; then
      exec_path="$probe"
      warn "[$preset] '$EXE_NAME' not found; use detected: $(basename "$probe")"
    else
      warn "[$preset] no executable, skip."
      continue
    fi
  fi

  FOUND=$((FOUND+1))

  # 检测产物架构
  BIN_ARCH="$(elf_arch "$exec_path")"
  # 静态 or 动态
  if is_dynamic "$exec_path"; then
    # 如果能明确看出来是另一个架构，而且 ldd 跑不动，就跳过
    if [[ "$BIN_ARCH" != unknown && "$BIN_ARCH" != "$HOST_ARCH" ]]; then
      if ! ldd "$exec_path" >/dev/null 2>&1; then
        warn "[$preset] dynamic & cross-arch ($BIN_ARCH vs $HOST_ARCH), and ldd failed -> skip"
        SKIPPED+=("$preset (dynamic, $BIN_ARCH) — host is $HOST_ARCH")
        continue
      fi
    fi
    bundle_dynamic "$preset_dir" "$preset" "$exec_path"
    PROCESSED=$((PROCESSED+1))
  else
    bundle_static "$preset_dir" "$preset" "$exec_path"
    PROCESSED=$((PROCESSED+1))
  fi

done < <(find "$ROOT/build" -mindepth 1 -maxdepth 1 -type d -name 'microear-*-linux-*' -print | sort)

# =========================
# 11. 总结
# =========================
echo
log "SUMMARY: total found presets: $FOUND, processed: $PROCESSED, skipped: ${#SKIPPED[@]}"
if ((${#SKIPPED[@]})); then
  printf '%s\n' "${SKIPPED[@]}" | sed 's/^/ - /'
fi

echo
log "NEXT:"
echo " - 构建某个镜像:   install/<preset>/build-image.sh"
echo " - 运行某个镜像:   install/<preset>/run-image.sh"
echo " - 动态库校验:     install/<preset>/ld-list.sh   # 仅动态"
