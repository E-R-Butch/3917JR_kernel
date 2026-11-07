#!/usr/bin/env bash
# 3917JR 内核标准构建脚本（容器内运行）
# 用法：OUT=/root/kbuild LV=-gaea-R46 bash /root/kernel/build-gaea.sh [Image|modules|all]
#
# 可复现约定（2026-11-07 实测补齐）：同一 commit、任意 O= 目录、任意时间两次构建，
# Image 必须逐字节相同。此前有 37 字节漂移，全部来自两处：
#   ① 横幅里的构建时刻（KBUILD_BUILD_TIMESTAMP 未固定）
#   ② build-id（裸 --build-id 摘要含构建路径）
# 现改为：时刻取 HEAD 的提交时间、build-id 取 HEAD 的 sha，两者都由 commit 决定。
set -uo pipefail

export PATH=/root/tooling/toolchain/bin:/root/tooling/deps/binutils234/usr/bin:/root/tooling/deps/bin:/usr/local/bin:/usr/bin:/bin
export LD_LIBRARY_PATH=/root/tooling/deps/binutils234/usr/lib/x86_64-linux-gnu:/root/tooling/deps/root/usr/lib/x86_64-linux-gnu:/root/tooling/toolchain/lib64
ln -sf /root/tooling/toolchain/bin/clang /usr/local/bin/clang.real 2>/dev/null
ln -sf /usr/bin/python3 /usr/local/bin/python 2>/dev/null

OUT="${OUT:-/root/kbuild}"
LV="${LV:--gaea-R46}"
TARGET="${1:-all}"
case "$TARGET" in
  Image)   GOAL="Image" ;;
  modules) GOAL="modules" ;;
  *)       GOAL="Image modules" ;;
esac

cd /root/kernel || exit 9

# 构建标识：/proc/version 里的 user@host #N
export KBUILD_BUILD_USER=sinclair
export KBUILD_BUILD_HOST=zenith
export KBUILD_BUILD_VERSION="${KBUILD_BUILD_VERSION:-17}"

# 可复现钉桩：时刻与 build-id 都从 HEAD 派生，换 commit 才变，重跑不变
HEAD_SHA=$(git rev-parse HEAD)
HEAD_TS=$(git log -1 --format=%ct)
export KBUILD_BUILD_TIMESTAMP="$(date -u -d "@$HEAD_TS" '+%a %b %e %T %Y')"
BUILD_ID_FLAG="--build-id=0x${HEAD_SHA}"

echo "== 配置 =="
[ -f "$OUT/.config" ] || { echo "缺少 $OUT/.config（新 out 目录先 cp 一份再跑）"; exit 8; }
scripts/config --file "$OUT/.config" --set-str LOCALVERSION "$LV"
grep -E '^CONFIG_LOCALVERSION=' "$OUT/.config"
echo "HEAD=$HEAD_SHA  TIMESTAMP=$KBUILD_BUILD_TIMESTAMP  BUILD_ID=$BUILD_ID_FLAG"

echo "== 构建 $GOAL -> $OUT =="
make O="$OUT" -j8 ARCH=arm64 \
     CC=clang.real \
     LD=aarch64-linux-gnu-ld \
     CLANG_TRIPLE=aarch64-linux-gnu- \
     CROSS_COMPILE=aarch64-linux-gnu- \
     HOSTCFLAGS="-fcommon" \
     LDFLAGS_BUILD_ID="$BUILD_ID_FLAG" \
     $GOAL > "$OUT/build.log" 2>&1
rc=$?
tail -5 "$OUT/build.log"
[ $rc -ne 0 ] && { echo "BUILD FAILED rc=$rc"; grep -nE 'error:|Error [0-9]' "$OUT/build.log" | head -5; exit $rc; }

echo "== 产物 =="
ls -l "$OUT/arch/arm64/boot/Image" | awk '{print "Image:", $5}'
sha256sum "$OUT/arch/arm64/boot/Image"
strings "$OUT/arch/arm64/boot/Image" | grep -m1 "Linux version"
