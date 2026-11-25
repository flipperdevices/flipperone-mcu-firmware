"""Convert a PNG into a C `Image` (see applications/services/gui/image.h).

The format is picked from the picture:
- only pure black and white opaque pixels: ImageFormatMono1, 1 bit per pixel;
- anything else: ImageFormatRawGray8, 1 byte per pixel.
Fully transparent pixels (alpha 0) add a 1 bit per pixel mask to either format.
Partial transparency cannot be drawn and fails the build.
"""

import sys

from PIL import Image


def pack_bits(bits, width, height):
    """Pack rows of 0/1 values MSB first, each row padded to a whole byte."""
    stride = (width + 7) // 8
    packed = bytearray(stride * height)
    for y in range(height):
        row = y * width
        for x in range(width):
            if bits[row + x]:
                packed[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(packed)


def write_array(f, name, data, per_line):
    f.write(f"static const uint8_t {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), per_line):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + per_line])
        f.write(f"    {chunk},\n")
    f.write("};\n\n")


def png_to_c_array(input_png, output_c, var_name):
    img = Image.open(input_png).convert("RGBA")
    width, height = img.size
    alpha = list(img.getchannel("A").get_flattened_data())
    gray = list(img.convert("L").get_flattened_data())

    partial = sum(1 for a in alpha if a not in (0, 255))
    if partial:
        sys.exit(
            f"{input_png}: {partial} pixel(s) are partially transparent; "
            "only fully opaque or fully transparent pixels can be drawn"
        )

    opaque = [a == 255 for a in alpha]
    mono = all(g in (0, 255) for g, o in zip(gray, opaque) if o)
    stride = (width + 7) // 8

    with open(output_c, "w") as f:
        f.write("#include <gui/image.h>\n\n")

        if mono:
            # A set bit is ink (black); transparent pixels are left clear
            data = pack_bits([o and g == 0 for g, o in zip(gray, opaque)], width, height)
            write_array(f, f"{var_name}_data", data, stride)
            image_format = "ImageFormatMono1"
        else:
            write_array(f, f"{var_name}_data", bytes(gray), width)
            image_format = "ImageFormatRawGray8"

        mask = "NULL"
        if not all(opaque):
            write_array(f, f"{var_name}_mask", pack_bits(opaque, width, height), stride)
            mask = f"{var_name}_mask"

        f.write(f"const Image {var_name} = {{\n")
        f.write(f"    .format = {image_format},\n")
        f.write(f"    .width = {width},\n")
        f.write(f"    .height = {height},\n")
        f.write(f"    .data = {var_name}_data,\n")
        f.write(f"    .mask = {mask},\n")
        f.write("};\n")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} input.png output.c variable_name")
        sys.exit(1)
    png_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
