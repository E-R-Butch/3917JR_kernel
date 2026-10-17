#!/usr/bin/env bash
# 3917JR 内核标准构建脚本（容器内运行）
# 用法：bash /root/build.sh [Image|modules|all]
set -uo pipefail

export PATH=/root/tooling/toolchain/bin:/root/tooling/deps/binutils234/usr/bin:/root/tooling/deps/bin:/usr/local/bin:/usr/bin:/bin
export LD_LIBRARY_PATH=/root/tooling/deps/binutils234/usr/lib/x86_64-linux-gnu:/root/tooling/deps/root/usr/lib/x86_64-linux-gnu:/root/tooling/toolchain/lib64
ln -sf /root/tooling/toolchain/bin/clang /usr/local/bin/clang.real 2>/dev/null
ln -sf /usr/bin/python3 /usr/local/bin/python 2>/dev/null

# 构建标识：/proc/version 里的 user@host
export KBUILD_BUILD_USER=sinclair
export KBUILD_BUILD_HOST=zenith
export KBUILD_BUILD_VERSION=17

TARGET="${1:-all}"
case "$TARGET" in
  Image)   GOAL="Image" ;;
  modules) GOAL="modules" ;;
  *)       GOAL="Image modules" ;;
esac

cd /root/kernel || exit 9
echo "== 配置 =="
grep -E '^CONFIG_LOCALVERSION=' /root/kbuild/.config

echo "== 构建 $GOAL =="
make O=/root/kbuild -j8 ARCH=arm64 \
     CC=clang.real \
     LD=aarch64-linux-gnu-ld \
     CLANG_TRIPLE=aarch64-linux-gnu- \
     CROSS_COMPILE=aarch64-linux-gnu- \
     HOSTCFLAGS="-fcommon" \
     $GOAL 2>&1 | tail -5

echo "== 产物 =="
ls -l /root/kbuild/arch/arm64/boot/Image | awk '{print "Image:", $5}'
strings /root/kbuild/arch/arm64/boot/Image | grep -m1 "Linux version"
