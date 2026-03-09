import sys
from PIL import Image
import os

def png_to_c_array(input_png, output_c, var_name):
    img = Image.open(input_png).convert("L")
    width, height = img.size
    pixels = list(img.get_flattened_data())

    with open(output_c, "w") as f:
        f.write(f"#include <gui/image.h>\n\n")
        f.write(f"static const uint8_t {var_name}_data[{width * height}] = {{\n")
        for i, px in enumerate(pixels):
            if i % width == 0:
                f.write("    ")
            f.write(f"0x{px:02X}")
            if i < len(pixels) - 1:
                f.write(", ")
            if i % width == width - 1:
                f.write("\n")
        f.write("};\n\n")
        f.write(f"const Image {var_name} = {{\n")
        f.write(f"    .format = ImageFormatRawGray8,\n")
        f.write(f"    .width = {width},\n")
        f.write(f"    .height = {height},\n")
        f.write(f"    .data = {var_name}_data,\n")
        f.write(f"}};\n")

    print(f"Converted {input_png} ({width}x{height}) -> {output_c}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} input.png output.c variable_name")
        sys.exit(1)
    png_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
