import sys
from PIL import Image
import os

# A PNG pixel with alpha below this is left out of the image (mask bit clear).
# Pixel art is either opaque or clear; anything in between is a stray edge.
OPAQUE_ALPHA = 128


def write_gray8_array(f, var_name, img):
    """Write `img` as a `static const uint8_t <var_name>[]` RawGray8 array.

    Returns the (width, height) of the image.
    """
    img = img.convert("L")
    width, height = img.size
    pixels = list(img.get_flattened_data())

    f.write(f"static const uint8_t {var_name}[{width * height}] = {{\n")
    for i, px in enumerate(pixels):
        if i % width == 0:
            f.write("    ")
        f.write(f"0x{px:02X}")
        if i < len(pixels) - 1:
            f.write(", ")
        if i % width == width - 1:
            f.write("\n")
    f.write("};\n\n")
    return width, height


def transparency_mask(img):
    """The 1-bit mask of `img` (MSB first, rows padded to a byte), or None if
    every pixel is opaque and the plain RawGray8 format is enough."""
    if img.mode not in ("RGBA", "LA") and "transparency" not in img.info:
        return None
    alpha = list(img.convert("RGBA").getchannel("A").get_flattened_data())
    if all(a >= OPAQUE_ALPHA for a in alpha):
        return None

    width, height = img.size
    stride = (width + 7) // 8
    mask = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if alpha[y * width + x] >= OPAQUE_ALPHA:
                mask[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(mask)


def write_gray8_mask_array(f, var_name, img, mask):
    """Write `img` as a RawGray8Mask array: the pixels, then the mask rows."""
    img = img.convert("L")
    width, height = img.size
    pixels = list(img.get_flattened_data())
    stride = (width + 7) // 8

    f.write(f"static const uint8_t {var_name}[{width * height + len(mask)}] = {{\n")
    for y in range(height):
        f.write("    " + ", ".join(f"0x{px:02X}" for px in pixels[y * width:(y + 1) * width]) + ",\n")
    f.write("    // transparency mask\n")
    for y in range(height):
        row = mask[y * stride:(y + 1) * stride]
        f.write("    " + ", ".join(f"0x{b:02X}" for b in row) + ",\n")
    f.write("};\n\n")
    return width, height


def write_image(f, var_name, img, storage_class="static "):
    """Write `img` as `<storage_class>const Image <var_name>` with its pixel
    array, in the RawGray8 or RawGray8Mask format depending on transparency."""
    mask = transparency_mask(img)
    if mask is None:
        width, height = write_gray8_array(f, f"{var_name}_data", img)
        fmt = "ImageFormatRawGray8"
    else:
        width, height = write_gray8_mask_array(f, f"{var_name}_data", img, mask)
        fmt = "ImageFormatRawGray8Mask"
    f.write(f"{storage_class}const Image {var_name} = {{\n")
    f.write(f"    .format = {fmt},\n")
    f.write(f"    .width = {width},\n")
    f.write(f"    .height = {height},\n")
    f.write(f"    .data = {var_name}_data,\n")
    f.write(f"}};\n")


def png_to_c_array(input_png, output_c, var_name):
    img = Image.open(input_png)

    with open(output_c, "w") as f:
        f.write(f"#include <gui/image.h>\n\n")
        write_image(f, var_name, img, storage_class="")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} input.png output.c variable_name")
        sys.exit(1)
    png_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
