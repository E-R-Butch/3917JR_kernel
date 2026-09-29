#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
kernel="$repo_root/msm-4.19"
config_dir="$repo_root/reconstruction/configs"
out=${OUT:-"$repo_root/out/gaea-reconstructed"}
toolchain=${TOOLCHAIN:-}

if [[ -z "$toolchain" || ! -x "$toolchain/bin/clang" ]]; then
  echo "Set TOOLCHAIN to an Android Clang prebuilt directory (r365631c recommended)." >&2
  exit 1
fi

export PATH="$toolchain/bin:$PATH"

apply_fragment() {
  local fragment=$1
  while IFS= read -r line; do
    case "$line" in
      CONFIG_*=y)
        "$kernel/scripts/config" --file "$out/.config" --enable "${line%%=*}"
        ;;
      CONFIG_*=m)
        "$kernel/scripts/config" --file "$out/.config" --module "${line%%=*}"
        ;;
      CONFIG_*=n)
        "$kernel/scripts/config" --file "$out/.config" --disable "${line%%=*}"
        ;;
    esac
  done < "$fragment"
}

mkdir -p "$out"
cp "$config_dir/gaea-stock.config" "$out/.config"
apply_fragment "$config_dir/reconstructed.config"
apply_fragment "$config_dir/droidspaces-4.19.config"

make_args=(
  -C "$kernel"
  O="$out"
  ARCH=arm64
  CC=clang
  LD=aarch64-linux-gnu-ld
  CLANG_TRIPLE=aarch64-linux-gnu-
  CROSS_COMPILE=aarch64-linux-gnu-
)

clang --version
aarch64-linux-gnu-ld --version
make "${make_args[@]}" olddefconfig

required=(
  CONFIG_SYSVIPC
  CONFIG_POSIX_MQUEUE
  CONFIG_PID_NS
  CONFIG_UTS_NS
  CONFIG_IPC_NS
  CONFIG_CGROUP_DEVICE
  CONFIG_DEVTMPFS
  CONFIG_VETH
  CONFIG_TOUCHSCREEN_FTS
)

for symbol in "${required[@]}"; do
  if ! grep -qx "$symbol=y" "$out/.config"; then
    echo "Required option was not enabled: $symbol" >&2
    exit 1
  fi
done

"$kernel/scripts/diffconfig" "$config_dir/gaea-stock.config" "$out/.config" \
  > "$out/gaea-reconstructed-droidspaces.config.diff"
cp "$out/.config" "$out/gaea-reconstructed-droidspaces.config"

make "${make_args[@]}" -j"${JOBS:-8}" Image

sha256sum \
  "$out/arch/arm64/boot/Image" \
  "$out/gaea-reconstructed-droidspaces.config"
