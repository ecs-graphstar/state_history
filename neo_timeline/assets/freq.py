import os
from PIL import Image

def get_most_frequent_non_transparent(image_path):
    try:
        with Image.open(image_path) as img:
            # Ensure image is in RGBA to access transparency data
            img = img.convert('RGBA')

            # Resize for performance
            img.thumbnail((100, 100))

            # Get list of (count, (R, G, B, A))
            colors = img.getcolors(maxcolors=1000000)

            if not colors:
                return "No pixels found"

            # Filter out pixels where Alpha is 0 (fully transparent)
            non_transparent = [c for c in colors if c[1][3] > 0]

            if not non_transparent:
                return "Fully Transparent"

            # Sort by count descending and pick the top one
            most_frequent = max(non_transparent, key=lambda item: item[0])

            # Extract RGB (ignore A for the hex output)
            r, g, b, a = most_frequent[1]
            return '#{:02x}{:02x}{:02x}'.format(r, g, b)

    except Exception as e:
        return f"Error: {e}"

def process_directory(directory_path, output_file):
    # Only check formats that likely support transparency
    supported_formats = ('.png', '.webp', '.bmp', '.tiff', '.jpg', '.jpeg')

    with open(output_file, 'w') as f:
        f.write("Filename, Hex_Color\n")

        for filename in os.listdir(directory_path):
            if filename.lower().endswith(supported_formats):
                file_path = os.path.join(directory_path, filename)
                hex_color = get_most_frequent_non_transparent(file_path)

                f.write(f"{filename}, {hex_color}\n")
                print(f"Processed: {filename} -> {hex_color}")

if __name__ == "__main__":
    target_dir = "./food"  # Change this to your folder
    results_log = "frequent_colors.txt"

    process_directory(target_dir, results_log)
    print(f"\nReport generated: {results_log}")
