#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PYTHON=${PYTHON:-python3}
REQUIRED_FONTTOOLS_VERSION=${REQUIRED_FONTTOOLS_VERSION:-4.63.0}
REQUIRED_PILLOW_VERSION=${REQUIRED_PILLOW_VERSION:-12.3.0}
export REQUIRED_FONTTOOLS_VERSION REQUIRED_PILLOW_VERSION
VERSION=Sans2.004
ARCHIVE=08_NotoSansCJKsc.zip
URL="https://github.com/notofonts/noto-cjk/releases/download/$VERSION/$ARCHIVE"
SHA256=a927e56f53bd6c3b920bc139c0b94aa36c7d9ad0cf009b159437a1a003581140
NOTO_CJK_COMMIT=f8d157532fbfaeda587e826d4cd5b21a49186f7c
NOTO_SC_LYRIC_FONT=NotoSansCJKsc-Medium.otf
NOTO_SC_LYRIC_URL="https://github.com/notofonts/noto-cjk/raw/$NOTO_CJK_COMMIT/Sans/OTF/SimplifiedChinese/$NOTO_SC_LYRIC_FONT"
NOTO_SC_LYRIC_SHA256=ca094f6b0001fb048ca39ddd797a0cdb0179e1e55c6561e111c49c3e6a61d7b7
NOTO_JP_FONT=NotoSansCJKjp-Medium.otf
NOTO_JP_URL="https://github.com/notofonts/noto-cjk/raw/$NOTO_CJK_COMMIT/Sans/OTF/Japanese/$NOTO_JP_FONT"
NOTO_JP_SHA256=dd523e580e3413c480b2d701bf64e534c20f8419e3cfb6a44c2bdcd8d2a6c052
MISANS_ARCHIVE=MiSans_Global_ALL.zip
MISANS_URL="https://hyperos.mi.com/font-download/$MISANS_ARCHIVE"
MISANS_SHA256=2322a01e3138661a3d76980cfd8167f704bbea60445df94a00c79d3d6ae85b57
MISANS_FONT_SHA256=5daf8d5447bfd423cfdec94a0e07c53b205892223ccc6ea21b7b8a37248b44d9
MISANS_LICENSE_URL="https://hyperos.mi.com/font-download/MiSans%E5%AD%97%E4%BD%93%E7%9F%A5%E8%AF%86%E4%BA%A7%E6%9D%83%E8%AE%B8%E5%8F%AF%E5%8D%8F%E8%AE%AE.pdf"
MISANS_LICENSE_SHA256=4a93a27cd2bd81b3b5ecfd0a853144a876fa26938a93a68443c67d74172fcb86
FREQUENCY_COMMIT=525f9b560de45753a5ea01069454e72e9aa541c6
FREQUENCY_URL="https://raw.githubusercontent.com/hermitdave/FrequencyWords/$FREQUENCY_COMMIT/content/2018/ko/ko_full.txt"
FREQUENCY_SHA256=a6495622eb095d1d4cf6e934199a5cb674fbba7d8179525e273e9fcd0d4ac325
OPENCC_COMMIT=81223ed87ae53283ef518e2deac34b7971f8a39e
OPENCC_DICTIONARY_URL="https://raw.githubusercontent.com/BYVoid/OpenCC/$OPENCC_COMMIT/data/dictionary/STCharacters.txt"
OPENCC_DICTIONARY_SHA256=81c27e6364fd164181276197b9215cf95f7f12a050aa207375248a5badf8d6fc
OPENCC_TW_VARIANTS_SHA256=48e694ad1ac43fd5927285e4fb3aa8a8dc9d9c065d6d3d314527c021a12839e2
OPENCC_HK_VARIANTS_SHA256=e5cd4345303224587102f2c9e4d2b67d2b7e349c6ce9152e4a118f4656cf7302
OPENCC_LICENSE_URL="https://raw.githubusercontent.com/BYVoid/OpenCC/$OPENCC_COMMIT/LICENSE"
OPENCC_LICENSE_SHA256=b534e465949558eec2597b04f5092b5e161236a68dfbfd04d547592ac3964308
KOREAN_SYLLABLES=3500
JAPANESE_LABEL=full-BMP-Japanese
IMMERSIVE_SHA256=4a413bbfd08792271f20a9f7e164cbef5504e5e8d864b43138a7b7b38451a383
IMMERSIVE_JP_SHA256=2c71ac61acb23371a123fc0b012bae4d5df01e4f620814fdf57ef0269be928a7
CONTENT_POINT_SHA256=d4cf740d9e26d5fb7ae493b8a2ea68cc6f6ce41a0d9177aca6c28d68452891d0
CONTENT_LARGE_POINT_SHA256=c414be80a173acc704f068412ecd004fa4be2f6e4045ac92f456b5aa6e4d35a0
UI_FONT_SHA256=1082b2e682a6b26ef39c9b70ae3c6c600eed51566c8a057fd9a8cfcbcbd34a04
UI_FONT_SIZE=9
DEVKITARM_IMAGE=${DEVKITARM_IMAGE:-devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5}
mkdir -p "$ROOT/data"
CACHE="$ROOT/data/downloads"
mkdir -p "$CACHE"
TMP=$(mktemp -d "$ROOT/data/content-font.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

verify_sha256() {
    local expected=$1
    local file=$2
    local label=$3
    local actual
    actual=$(shasum -a 256 "$file" | awk '{print $1}')
    if [[ "$actual" != "$expected" ]]; then
        echo "error: $label SHA-256 is $actual, expected $expected" >&2
        return 1
    fi
}

"$PYTHON" - <<'PY'
import sys
import os

if sys.version_info < (3, 10):
    raise SystemExit("error: font generation requires Python 3.10 or newer")
try:
    import fontTools
    import PIL
except ImportError as error:
    raise SystemExit(
        "error: font generation requires fonttools: " + str(error)
    )
if (fontTools.__version__ != os.environ["REQUIRED_FONTTOOLS_VERSION"] or
        PIL.__version__ != os.environ["REQUIRED_PILLOW_VERSION"]):
    raise SystemExit(
        "error: install pinned tools with: "
        "python3 -m pip install -r tools/font-requirements.txt"
    )
PY

if ! command -v mkbcfnt >/dev/null 2>&1 &&
   ! command -v docker >/dev/null 2>&1; then
    echo "error: mkbcfnt or Docker is required" >&2
    exit 1
fi

if [[ -f "$CACHE/$ARCHIVE" ]] &&
   verify_sha256 "$SHA256" "$CACHE/$ARCHIVE" "$ARCHIVE"; then
    cp "$CACHE/$ARCHIVE" "$TMP/$ARCHIVE"
else
    curl --proto '=https' --tlsv1.2 -fL --retry 3 \
        "$URL" -o "$TMP/$ARCHIVE"
    verify_sha256 "$SHA256" "$TMP/$ARCHIVE" "$ARCHIVE"
    cp "$TMP/$ARCHIVE" "$CACHE/$ARCHIVE"
fi
unzip -qj "$TMP/$ARCHIVE" NotoSansCJKsc-Regular.otf LICENSE -d "$TMP"

if [[ -f "$CACHE/$NOTO_SC_LYRIC_FONT" ]] &&
   verify_sha256 "$NOTO_SC_LYRIC_SHA256" \
       "$CACHE/$NOTO_SC_LYRIC_FONT" "$NOTO_SC_LYRIC_FONT"; then
    cp "$CACHE/$NOTO_SC_LYRIC_FONT" "$TMP/$NOTO_SC_LYRIC_FONT"
else
    curl --proto '=https' --tlsv1.2 -fL --retry 3 \
        "$NOTO_SC_LYRIC_URL" -o "$TMP/$NOTO_SC_LYRIC_FONT"
    verify_sha256 "$NOTO_SC_LYRIC_SHA256" \
        "$TMP/$NOTO_SC_LYRIC_FONT" "$NOTO_SC_LYRIC_FONT"
    cp "$TMP/$NOTO_SC_LYRIC_FONT" "$CACHE/$NOTO_SC_LYRIC_FONT"
fi

if [[ -f "$CACHE/$NOTO_JP_FONT" ]] &&
   verify_sha256 "$NOTO_JP_SHA256" "$CACHE/$NOTO_JP_FONT" \
       "$NOTO_JP_FONT"; then
    cp "$CACHE/$NOTO_JP_FONT" "$TMP/$NOTO_JP_FONT"
else
    curl --proto '=https' --tlsv1.2 -fL --retry 3 \
        "$NOTO_JP_URL" -o "$TMP/$NOTO_JP_FONT"
    verify_sha256 "$NOTO_JP_SHA256" "$TMP/$NOTO_JP_FONT" \
        "$NOTO_JP_FONT"
    cp "$TMP/$NOTO_JP_FONT" "$CACHE/$NOTO_JP_FONT"
fi

if [[ -f "$CACHE/$MISANS_ARCHIVE" ]] &&
   verify_sha256 "$MISANS_SHA256" "$CACHE/$MISANS_ARCHIVE" \
       "$MISANS_ARCHIVE"; then
    cp "$CACHE/$MISANS_ARCHIVE" "$TMP/$MISANS_ARCHIVE"
else
    curl --proto '=https' --tlsv1.2 -fL --retry 3 \
        "$MISANS_URL" -o "$TMP/$MISANS_ARCHIVE"
    verify_sha256 "$MISANS_SHA256" "$TMP/$MISANS_ARCHIVE" \
        "$MISANS_ARCHIVE"
    cp "$TMP/$MISANS_ARCHIVE" "$CACHE/$MISANS_ARCHIVE"
fi
unzip -p "$TMP/$MISANS_ARCHIVE" \
    'MiSans Global _ALL/MiSans.zip' > "$TMP/MiSans.zip"
unzip -p "$TMP/MiSans.zip" \
    'MiSans/MiSans VF.ttf' > "$TMP/MiSans-VF.ttf"
verify_sha256 "$MISANS_FONT_SHA256" "$TMP/MiSans-VF.ttf" \
    "MiSans VF.ttf"
curl --proto '=https' --tlsv1.2 -fL --retry 3 \
    "$MISANS_LICENSE_URL" -o "$TMP/MiSans-License.pdf"
verify_sha256 "$MISANS_LICENSE_SHA256" "$TMP/MiSans-License.pdf" \
    "MiSans license"

"$PYTHON" "$ROOT/tools/gen_ui_font_whitelist.py" \
    --font "$TMP/MiSans-VF.ttf" \
    --source "$ROOT/source/i18n.c" \
    --source "$ROOT/source/ui.c" \
    --output "$TMP/ui-font-whitelist.txt"

if command -v mkbcfnt >/dev/null 2>&1; then
    mkbcfnt -s "$UI_FONT_SIZE" -w "$TMP/ui-font-whitelist.txt" \
        -o "$TMP/ui-menu-font.bcfnt" "$TMP/NotoSansCJKsc-Regular.otf"
else
    TMP_REL=${TMP#"$ROOT"/}
    docker run --rm -v "$ROOT":/project -w /project \
        "$DEVKITARM_IMAGE" \
        mkbcfnt -s "$UI_FONT_SIZE" \
            -w "$TMP_REL/ui-font-whitelist.txt" \
            -o "$TMP_REL/ui-menu-font.bcfnt" \
            "$TMP_REL/MiSans-VF.ttf"
fi

"$PYTHON" "$ROOT/tools/normalize_bcfnt.py" "$TMP/ui-menu-font.bcfnt"

curl --proto '=https' --tlsv1.2 -fL --retry 3 \
    "$FREQUENCY_URL" -o "$TMP/ko_full.txt"
printf '%s  %s\n' "$FREQUENCY_SHA256" "$TMP/ko_full.txt" | \
    shasum -a 256 -c -
curl --proto '=https' --tlsv1.2 -fL --retry 3 \
    "$OPENCC_DICTIONARY_URL" -o "$TMP/STCharacters.txt"
printf '%s  %s\n' "$OPENCC_DICTIONARY_SHA256" \
    "$TMP/STCharacters.txt" | shasum -a 256 -c -
for variant in TWVariants HKVariants; do
    curl --proto '=https' --tlsv1.2 -fL --retry 3 \
        "https://raw.githubusercontent.com/BYVoid/OpenCC/$OPENCC_COMMIT/data/dictionary/$variant.txt" \
        -o "$TMP/$variant.txt"
done
printf '%s  %s\n' "$OPENCC_TW_VARIANTS_SHA256" \
    "$TMP/TWVariants.txt" | shasum -a 256 -c -
printf '%s  %s\n' "$OPENCC_HK_VARIANTS_SHA256" \
    "$TMP/HKVariants.txt" | shasum -a 256 -c -
curl --proto '=https' --tlsv1.2 -fL --retry 3 \
    "$OPENCC_LICENSE_URL" -o "$TMP/OpenCC-LICENSE"
printf '%s  %s\n' "$OPENCC_LICENSE_SHA256" \
    "$TMP/OpenCC-LICENSE" | shasum -a 256 -c -

"$PYTHON" "$ROOT/tools/gen_content_font_whitelist.py" \
    --font "$TMP/MiSans-VF.ttf" \
    --fallback-font "$TMP/NotoSansCJKsc-Regular.otf" \
    --dictionary "$ROOT/romfs/pinyin_dict.bin" \
    --source "$ROOT/source/ui.c" \
    --extra "$ROOT/tools/content-font-extra-chars.txt" \
    --traditional-map "$TMP/STCharacters.txt" \
    --traditional-map "$TMP/TWVariants.txt" \
    --traditional-map "$TMP/HKVariants.txt" \
    --output "$TMP/content-font-whitelist.txt"

"$PYTHON" "$ROOT/tools/gen_korean_font_whitelist.py" \
    --font "$TMP/NotoSansCJKsc-Regular.otf" \
    --frequency "$TMP/ko_full.txt" \
    --target "$KOREAN_SYLLABLES" \
    --output "$TMP/korean-font-whitelist.txt"

"$PYTHON" "$ROOT/tools/gen_japanese_font_whitelist.py" \
    --output "$TMP/japanese-font-whitelist.txt"

sort -n -u "$TMP/content-font-whitelist.txt" \
    "$TMP/korean-font-whitelist.txt" \
    "$TMP/japanese-font-whitelist.txt" \
    -o "$TMP/point-font-whitelist.txt"

"$PYTHON" "$ROOT/tools/gen_immersive_font.py" \
    --font "$TMP/$NOTO_SC_LYRIC_FONT" \
    --fallback-font "$TMP/MiSans-VF.ttf" \
    --whitelist "$TMP/point-font-whitelist.txt" \
    --include-font-coverage \
    --baseline 24 \
    --alphabetic-baseline-shift 1 \
    --output "$TMP/immersive-font.bin"

"$PYTHON" "$ROOT/tools/gen_immersive_font.py" \
    --font "$TMP/$NOTO_JP_FONT" \
    --fallback-font "$TMP/MiSans-VF.ttf" \
    --whitelist "$TMP/point-font-whitelist.txt" \
    --include-font-coverage \
    --baseline 24 \
    --alphabetic-baseline-shift 1 \
    --output "$TMP/immersive-font-jp.bin"

"$PYTHON" "$ROOT/tools/gen_immersive_font.py" \
    --font "$TMP/MiSans-VF.ttf" \
    --fallback-font "$TMP/NotoSansCJKsc-Regular.otf" \
    --whitelist "$TMP/point-font-whitelist.txt" \
    --font-pixels 12 --glyph-width 18 --glyph-height 24 --baseline 17 \
    --alpha-bits 2 \
    --output "$TMP/content-point-font.bin"

"$PYTHON" "$ROOT/tools/gen_immersive_font.py" \
    --font "$TMP/MiSans-VF.ttf" \
    --fallback-font "$TMP/NotoSansCJKsc-Regular.otf" \
    --whitelist "$TMP/point-font-whitelist.txt" \
    --font-pixels 15 --glyph-width 18 --glyph-height 24 --baseline 18 \
    --alpha-bits 2 \
    --output "$TMP/content-large-point-font.bin"

test -s "$TMP/immersive-font.bin"
test -s "$TMP/immersive-font-jp.bin"
test -s "$TMP/content-point-font.bin"
test -s "$TMP/content-large-point-font.bin"
test -s "$TMP/ui-menu-font.bcfnt"
hash_status=0
verify_sha256 "$IMMERSIVE_SHA256" "$TMP/immersive-font.bin" \
    "generated immersive-font.bin" || hash_status=1
verify_sha256 "$IMMERSIVE_JP_SHA256" "$TMP/immersive-font-jp.bin" \
    "generated immersive-font-jp.bin" || hash_status=1
verify_sha256 "$CONTENT_POINT_SHA256" "$TMP/content-point-font.bin" \
    "generated content-point-font.bin" || hash_status=1
verify_sha256 "$CONTENT_LARGE_POINT_SHA256" \
    "$TMP/content-large-point-font.bin" \
    "generated content-large-point-font.bin" || hash_status=1
verify_sha256 "$UI_FONT_SHA256" "$TMP/ui-menu-font.bcfnt" \
    "generated ui-menu-font.bcfnt" || hash_status=1
if [[ "$hash_status" != 0 ]]; then
    exit 1
fi
mv "$TMP/immersive-font.bin" "$ROOT/romfs/immersive-font.bin"
mv "$TMP/immersive-font-jp.bin" \
    "$ROOT/romfs/immersive-font-jp.bin"
mv "$TMP/content-point-font.bin" "$ROOT/romfs/content-point-font.bin"
mv "$TMP/content-large-point-font.bin" \
    "$ROOT/romfs/content-large-point-font.bin"
mv "$TMP/ui-menu-font.bcfnt" "$ROOT/romfs/ui-menu-font.bcfnt"
cp "$TMP/LICENSE" "$ROOT/third_party/Noto-Sans-CJK-OFL-1.1.txt"
cp "$TMP/MiSans-License.pdf" "$ROOT/third_party/MiSans-Font-License.pdf"
cp "$TMP/OpenCC-LICENSE" "$ROOT/third_party/OpenCC-Apache-2.0.txt"
immersive_size=$(wc -c < "$ROOT/romfs/immersive-font.bin" | tr -d ' ')
immersive_jp_size=$(wc -c < \
    "$ROOT/romfs/immersive-font-jp.bin" | tr -d ' ')
point_size=$(wc -c < "$ROOT/romfs/content-point-font.bin" | tr -d ' ')
large_point_size=$(wc -c < \
    "$ROOT/romfs/content-large-point-font.bin" | tr -d ' ')
ui_font_size=$(wc -c < "$ROOT/romfs/ui-menu-font.bcfnt" | tr -d ' ')
echo "installed MiSans VF wght 430 UI fonts with Noto Sans CJK fallback"
echo "  12px 2-bit alpha glyphs in 18px Chinese/Latin/${KOREAN_SYLLABLES}-Hangul/${JAPANESE_LABEL} content cells: $point_size bytes"
echo "  15px 2-bit alpha glyphs for semantic titles: $large_point_size bytes"
echo "  24px Noto Sans CJK SC Medium multilingual immersive lyrics: $immersive_size bytes"
echo "  24px Noto Sans CJK JP Medium Japanese immersive lyrics: $immersive_jp_size bytes"
echo "  ${UI_FONT_SIZE}pt A4 fixed UI BCFNT: $ui_font_size bytes"
