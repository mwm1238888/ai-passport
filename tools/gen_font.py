#!/usr/bin/env python3
"""Regenerate main/fonts/wc_cn_20.c for the Work Companion UI.

Usage: python tools/gen_font.py   (requires Node.js for lv_font_conv)

Collects every non-ASCII character used by wc_ui.c UI strings plus the
QWeather condition vocabulary, then renders a 20px/bpp2 simhei font.
After changing UI text, re-run this script and rebuild.
"""
import subprocess, os, sys

STRINGS = [
    "工作伴侣", "天气", "鸡汤", "提醒", "录音", "设置",
    "电", "下页", "刷新", "需配",
    "晴", "多云", "阴", "阵雨", "雷阵雨伴有冰雹", "小雨", "中雨",
    "大雨", "暴雨", "大暴雨", "特大暴雨", "阵雪", "小雪", "中雪", "大雪", "暴雪", "雨夹雪",
    "雨雪天气", "阵雨夹雪", "冻雨", "沙尘暴", "浮尘", "扬沙", "强沙尘暴", "雾", "浓雾",
    "强浓雾", "轻雾", "大雾", "特强浓雾", "霾", "轻度霾", "中度霾", "重度霾", "严重霾",
    "热带低压", "热带风暴", "强热带风暴", "台风", "强台风", "超强台风", "冷", "暖", "未知", "夜",
    "换一句",
    "专注当下，一步即达。",
    "水滴石穿，非一日之功。",
    "今日事，今日毕。",
    "难者不会，会者不难。",
    "行动是治愈焦虑的良药。",
    "慢一点，但别停下来。",
    "把大事拆小，把小事做完。",
    "休息也是工作的一部分。",
    "闹铃", "久坐", "喝水", "下班", "时间到会弹窗",
    "该起床啦！", "久坐了，动一动", "喝口水吧", "下班辛苦啦！", "任意键关闭",
    "文件", "开始", "停", "长按播放",
    "语录", "见", "调参",
    "，。！？：、°℃",
]

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "main", "fonts", "wc_cn_20.c")
FONT = os.environ.get("WC_FONT_SRC", "C:/Windows/Fonts/simhei.ttf")

chars = set()
for s in STRINGS:
    for ch in s:
        if ord(ch) > 0x7F:
            chars.add(ch)
symbols = "".join(sorted(chars))
print(f"unique glyphs: {len(symbols)}")
if not os.path.exists(FONT):
    sys.exit(f"font not found: {FONT} (set WC_FONT_SRC to a CJK ttf)")

cmd = [
    "npx", "--yes", "lv_font_conv",
    "--font", FONT,
    "--size", "20", "--bpp", "2",
    "--format", "lvgl", "--no-compress",
    "--lv-font-name", "wc_cn_20",
    "--range", "0x20-0x7F,0xB0",
    "--symbols", symbols,
    "-o", OUT,
]
os.makedirs(os.path.dirname(OUT), exist_ok=True)
r = subprocess.run(cmd, shell=(os.name == "nt"), capture_output=True, text=True)
if r.returncode != 0:
    sys.exit(f"lv_font_conv failed:\n{r.stdout}\n{r.stderr}")
print(f"generated {OUT} ({os.path.getsize(OUT)} bytes)")
