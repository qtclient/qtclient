# Animated UI assets (qtclient tip)

Qt's PNG handler reads **static** PNG only (no `acTL` / APNG). Animated CIP UI art ships as **animated WebP** and loads through `QImageReader` (`imageCount`, `nextImageDelay`, `loopCount`).

When upstream ships new APNG PNGs under `data/`:

```bash
python3 tools/extract/apng_to_webp.py
python3 tools/extract/apng_to_webp.py --check   # CI: fail if any acTL .png remains
```

Update any hard-coded `:/…/*.png` image-strip paths to `.webp` when the source was APNG. Runtime paths without an extension resolve `.webp` before `.png` (`Image::load`).

GIF is supported the same way if Qt's GIF image plugin is present; WebP is preferred (lossless, smaller).
