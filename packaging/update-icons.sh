#!/bin/sh
# 从 icons/app.svg 重新生成 icons/png/ 下的多尺寸位图。
# 打包时这些 PNG 会随 scalable SVG 一起装进 hicolor 图标主题。
#
# 改了图标之后跑一次即可：  ./packaging/update-icons.sh
set -eu

cd "$(dirname "$0")/.."

if ! command -v rsvg-convert >/dev/null 2>&1; then
    echo "需要 rsvg-convert，请先安装：sudo apt install librsvg2-bin" >&2
    exit 1
fi

mkdir -p icons/png
for size in 32 48 64 128 256 512; do
    rsvg-convert -w "$size" -h "$size" -o "icons/png/mcl-$size.png" icons/app.svg
done

printf '已重新生成：\n'
ls -l icons/png
