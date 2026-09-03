#!/usr/bin/env python3
"""AresVPN Client - regenerate every brand raster from the two masters in this directory.

The masters (icon-master.png 1254x1254, logo-master.png 1672x941) are the operator's artwork:
white mark on solid black. Every derived file is written IN PLACE under the filename upstream
already references, so no .qrc, .rc, CMake, manifest or QML line changes for an image
(AresProject docs/reviews/2026-09-04-aresvpn-client-fork-survey.md, sections 2.3 E and 3.2).

Run from the repository root:  python branding/aresvpn/generate.py
Needs Pillow.  Deterministic: same masters, same bytes (Pillow's PNG writer is deterministic
for a given version; the version is printed so a diff can be explained).
"""
import io
import base64
import os
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps
import PIL

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
BG = (14, 14, 17, 255)  # #0E0E11 - upstream's ic_launcher_background, kept as the dark ground


def keyed(master: Path) -> Image.Image:
    """White-on-black master -> white RGBA with alpha = luminance (the black keys out)."""
    im = Image.open(master).convert("RGB")
    lum = ImageOps.grayscale(im)
    out = Image.new("RGBA", im.size, (255, 255, 255, 0))
    out.putalpha(lum)
    return out


def crop_to_glyph(im: Image.Image, pad: float) -> Image.Image:
    """Crop an RGBA image to its alpha bounding box, then add `pad` (fraction of the box) around."""
    box = im.getchannel("A").getbbox()
    l, t, r, b = box
    w, h = r - l, b - t
    p = int(round(max(w, h) * pad))
    side = max(w, h) + 2 * p
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(im.crop(box), ((side - w) // 2, (side - h) // 2), im.crop(box))
    return canvas


def fit(im: Image.Image, size: tuple[int, int], scale: float = 1.0) -> Image.Image:
    """Fit im into size (keeping aspect), scaled by `scale` of the box, centred, transparent."""
    W, H = size
    bw, bh = int(W * scale), int(H * scale)
    r = min(bw / im.width, bh / im.height)
    resized = im.resize((max(1, int(im.width * r)), max(1, int(im.height * r))), Image.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(resized, ((W - resized.width) // 2, (H - resized.height) // 2), resized)
    return canvas


def on_ground(im: Image.Image, colour=BG) -> Image.Image:
    ground = Image.new("RGBA", im.size, colour)
    ground.alpha_composite(im)
    return ground


def circle_mask(im: Image.Image) -> Image.Image:
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, im.width - 1, im.height - 1), fill=255)
    out = im.copy()
    out.putalpha(mask)
    return out


def save(im: Image.Image, rel: str, **kw):
    p = ROOT / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    im.save(p, **kw)
    print(f"  {rel}  {im.width}x{im.height}")


def main():
    print(f"Pillow {PIL.__version__}")
    glyph = crop_to_glyph(keyed(HERE / "icon-master.png"), pad=0.10)   # white mark, transparent
    logo = crop_to_glyph(keyed(HERE / "logo-master.png"), pad=0.0)     # square canvas - use bbox instead
    logo_bbox = keyed(HERE / "logo-master.png")
    lb = logo_bbox.getchannel("A").getbbox()
    logo = logo_bbox.crop(lb)                                           # mark + wordmark, transparent

    # --- desktop (client/images) -----------------------------------------------------------
    print("desktop:")
    ico_sizes = [16, 32, 48, 64, 128, 256]
    ico_frames = [on_ground(fit(glyph, (s, s), 0.86)) for s in ico_sizes]
    save(ico_frames[-1], "client/images/app.ico", format="ICO", sizes=[(s, s) for s in ico_sizes],
         append_images=ico_frames[:-1])
    save(on_ground(fit(glyph, (256, 256), 0.86)), "client/images/icon.png")
    # the big logo: upstream's is 1440x1200, transparent, drawn on the dark first screen
    save(fit(logo, (1440, 1200), 0.84), "client/images/amneziaBigLogo.png")
    # tray: 200x200 template masks (setIsMask(true) - only the alpha matters)
    for name in ("active", "default", "error"):
        save(fit(glyph, (200, 200), 0.92), f"client/images/tray/{name}.png")
    # the 23x22 control glyph: an SVG carrying the mark as an embedded raster
    png = io.BytesIO()
    fit(glyph, (92, 88), 0.96).save(png, format="PNG")
    b64 = base64.b64encode(png.getvalue()).decode("ascii")
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
           'width="23" height="22" viewBox="0 0 23 22">'
           f'<image width="23" height="22" xlink:href="data:image/png;base64,{b64}"/></svg>\n')
    (ROOT / "client/images/controls/amnezia.svg").write_text(svg, encoding="utf-8")
    print("  client/images/controls/amnezia.svg  23x22 (embedded raster)")

    # --- android (client/android/res) --------------------------------------------------------
    print("android:")
    dens = {"ldpi": 0.75, "mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}
    for d, k in dens.items():
        s = int(48 * k)
        legacy = on_ground(fit(glyph, (s, s), 0.80))
        save(legacy, f"client/android/res/mipmap-{d}/icon.png")
        save(circle_mask(legacy), f"client/android/res/mipmap-{d}/icon_round.png")
    for d, k in dens.items():
        if d == "ldpi":
            continue  # upstream ships no ldpi foreground
        s = int(108 * k)
        # adaptive foreground: the safe zone is the inner 66dp circle -> keep the mark within ~56%
        save(fit(glyph, (s, s), 0.56), f"client/android/res/mipmap-{d}/ic_launcher_foreground.png")
    for d, (w, h) in {"mdpi": (160, 90), "hdpi": (240, 135), "xhdpi": (320, 180)}.items():
        save(on_ground(fit(logo, (w, h), 0.80)), f"client/android/res/mipmap-{d}/ic_banner.png")
    # the notification / tile / monochrome drawable: white mark on transparent, 24dp base
    for d, k in dens.items():
        if d == "ldpi":
            continue
        s = int(24 * k)
        save(fit(glyph, (s, s), 1.0), f"client/android/res/drawable-{d}/ic_amnezia_round.png")
    vec = ROOT / "client/android/res/drawable/ic_amnezia_round.xml"
    if vec.exists():
        vec.unlink()
        print("  removed client/android/res/drawable/ic_amnezia_round.xml (replaced by the rasters above)")
    for d in dens:
        dead = ROOT / f"client/android/res/drawable-{d}/logo.png"
        if dead.exists():
            dead.unlink()
            print(f"  removed client/android/res/drawable-{d}/logo.png (referenced by nothing)")


if __name__ == "__main__":
    os.chdir(ROOT)
    main()
