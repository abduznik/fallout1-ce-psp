
import struct
from PIL import Image

memstick = "/f/emulators/ppsspp/memstick"

# 1. Convert raw INDEX8 (432x320) + palette to PNG
with open(f"{memstick}/frame_movie_raw_index8.bin", "rb") as f:
    index8 = f.read()
with open(f"{memstick}/frame_movie_palette.bin", "rb") as f:
    pal_raw = f.read()

pal = []
for i in range(256):
    r = pal_raw[i*3] << 2
    g = pal_raw[i*3+1] << 2
    b = pal_raw[i*3+2] << 2
    pal.append((r, g, b))

print(f"INDEX8: {len(index8)} bytes, expected 138240 for 432x320")

img_movie = Image.new("RGB", (432, 320))
px = img_movie.load()
for y in range(320):
    for x in range(432):
        idx = index8[y * 432 + x]
        px[x, y] = pal[idx]
img_movie.save(f"{memstick}/frame_movie_raw.png")
print(f"Saved frame_movie_raw.png {img_movie.size}")

# 2. Convert raw RGB565 (640x480) to PNG
with open(f"{memstick}/frame_tex_rgb565.bin", "rb") as f:
    rgb565 = f.read()

img_tex = Image.new("RGB", (640, 480))
px_tex = img_tex.load()
for y in range(480):
    for x in range(640):
        offset = (y * 640 + x) * 2
        hi = rgb565[offset]
        lo = rgb565[offset + 1]
        pixel = (hi << 8) | lo
        r = ((pixel >> 11) & 0x1F) << 3
        g = ((pixel >> 5) & 0x3F) << 2
        b = (pixel & 0x1F) << 3
        px_tex[x, y] = (r, g, b)

img_tex.save(f"{memstick}/frame_tex_rgb565.png")
print(f"Saved frame_tex_rgb565.png {img_tex.size}")

# 3. Crop the tex image to 432x320 at (104,80) for comparison
crop = img_tex.crop((104, 80, 104+432, 80+320))
crop.save(f"{memstick}/frame_tex_cropped.png")
print(f"Saved frame_tex_cropped.png (cropped 432x320 from (104,80))")

# Compare pixel-by-pixel
diff_pixels = 0
total = 432 * 320
for y in range(320):
    for x in range(432):
        if img_movie.getpixel((x, y)) != crop.getpixel((x, y)):
            diff_pixels += 1

print(f"\n--- PIXEL COMPARISON ---")
print(f"Total pixels: {total}")
print(f"Different: {diff_pixels} ({100*diff_pixels/total:.1f}%)")
if diff_pixels == 0:
    print("IDENTICAL => Corruption is ALREADY in raw MVE decode output")
else:
    print("DIFFERENT => Raw decode is clean, corruption introduced during convert/scale")
