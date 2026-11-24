"""Turn a folder of PNG frames into one `const Image` of ImageFormatAnimation.

    animation_convert.py <frames_dir> <output.c> <variable_name>

Frames are every *.png in the folder, in name order, and must all have the
same size. An optional meta.json next to them sets the speed:

    {"fps": 4}

Without it the animation runs at DEFAULT_FPS. The frame period is rounded to
whole milliseconds, which is what the firmware's tick resolves.
"""

import json
import os
import sys

from PIL import Image

from image_convert import write_gray8_array

DEFAULT_FPS = 10


def frames_to_c_array(frames_dir, output_c, var_name):
    frame_files = sorted(f for f in os.listdir(frames_dir) if f.lower().endswith(".png"))
    if not frame_files:
        raise SystemExit(f"{frames_dir}: no *.png frames")

    meta_path = os.path.join(frames_dir, "meta.json")
    meta = {}
    if os.path.exists(meta_path):
        with open(meta_path) as f:
            meta = json.load(f)
    fps = meta.get("fps", DEFAULT_FPS)
    if not isinstance(fps, (int, float)) or fps <= 0:
        raise SystemExit(f"{meta_path}: fps must be a positive number, got {fps!r}")
    frame_duration_ms = max(1, round(1000 / fps))

    with open(output_c, "w") as f:
        f.write("#include <gui/image.h>\n\n")

        size = None
        for index, name in enumerate(frame_files):
            img = Image.open(os.path.join(frames_dir, name))
            if size is None:
                size = img.size
            elif img.size != size:
                raise SystemExit(f"{frames_dir}/{name}: size {img.size} differs from the first frame's {size}")
            write_gray8_array(f, f"{var_name}_frame_{index}", img)
        width, height = size

        f.write(f"static const uint8_t* const {var_name}_frames[{len(frame_files)}] = {{\n")
        for index in range(len(frame_files)):
            f.write(f"    {var_name}_frame_{index},\n")
        f.write("};\n\n")

        f.write(f"static const ImageAnimation {var_name}_animation = {{\n")
        f.write(f"    .frames = {var_name}_frames,\n")
        f.write(f"    .frame_count = {len(frame_files)},\n")
        f.write(f"    .frame_duration_ms = {frame_duration_ms},\n")
        f.write("};\n\n")

        f.write(f"const Image {var_name} = {{\n")
        f.write("    .format = ImageFormatAnimation,\n")
        f.write(f"    .width = {width},\n")
        f.write(f"    .height = {height},\n")
        f.write(f"    .data = &{var_name}_animation,\n")
        f.write("};\n")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} frames_dir output.c variable_name")
        sys.exit(1)
    frames_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
