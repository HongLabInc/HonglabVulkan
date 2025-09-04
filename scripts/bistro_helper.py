import os
import sys
from PIL import Image
import statistics
from pathlib import Path
import shutil

def get_image_files(folder_path):
    """Get all image files from the specified folder and subfolders."""
    image_extensions = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff', '.tif', '.webp'}
    image_files = []
    
    for root, dirs, files in os.walk(folder_path):
        for file in files:
            if Path(file).suffix.lower() in image_extensions:
                image_files.append(os.path.join(root, file))
    
    return image_files

def resize_image_if_needed(image, max_size=128):
    """Resize image if width or height exceeds max_size while maintaining aspect ratio."""
    width, height = image.size
    
    if width <= max_size and height <= max_size:
        return image
    
    # Calculate the scaling factor to fit within max_size x max_size
    scale_factor = min(max_size / width, max_size / height)
    new_width = int(width * scale_factor)
    new_height = int(height * scale_factor)
    
    return image.resize((new_width, new_height), Image.Resampling.LANCZOS)

def create_lowres_copy(source_folder, max_size=512):
    """Create a lowres folder with resized copies of all images."""
    lowres_folder = os.path.join(os.path.dirname(source_folder), f"LowRes")
    
    print(f"\nCreating lowres copy in: {lowres_folder}")
    
    # Create the lowres root folder if it doesn't exist
    os.makedirs(lowres_folder, exist_ok=True)
    
    image_files = get_image_files(source_folder)
    
    if not image_files:
        print("No image files found to copy.")
        return
    
    processed_count = 0
    resized_count = 0
    failed_count = 0
    
    for image_path in image_files:
        try:
            # Calculate relative path from source folder
            rel_path = os.path.relpath(image_path, source_folder)
            
            # Create destination path in lowres folder
            dest_path = os.path.join(lowres_folder, rel_path)
            
            # Create destination directory if it doesn't exist
            dest_dir = os.path.dirname(dest_path)
            os.makedirs(dest_dir, exist_ok=True)
            
            # Open, resize if needed, and save the image
            with Image.open(image_path) as img:
                original_size = img.size
                resized_img = resize_image_if_needed(img, max_size)
                
                # Convert RGBA to RGB if saving as JPEG
                if dest_path.lower().endswith(('.jpg', '.jpeg')) and resized_img.mode == 'RGBA':
                    # Create a white background
                    rgb_img = Image.new('RGB', resized_img.size, (255, 255, 255))
                    rgb_img.paste(resized_img, mask=resized_img.split()[3] if len(resized_img.split()) == 4 else None)
                    resized_img = rgb_img
                
                resized_img.save(dest_path, optimize=True)
                
                new_size = resized_img.size
                processed_count += 1
                
                if original_size != new_size:
                    resized_count += 1
                    print(f"  Resized: {rel_path} ({original_size[0]}x{original_size[1]} -> {new_size[0]}x{new_size[1]})")
                else:
                    print(f"  Copied: {rel_path} ({original_size[0]}x{original_size[1]})")
                    
        except Exception as e:
            failed_count += 1
            print(f"  Failed to process {rel_path}: {e}")
    
    print(f"\nLowres copy completed:")
    print(f"  Total images processed: {processed_count}")
    print(f"  Images resized: {resized_count}")
    print(f"  Images copied unchanged: {processed_count - resized_count}")
    if failed_count > 0:
        print(f"  Failed to process: {failed_count}")

def analyze_image_resolutions(folder_path):
    """Analyze image resolutions and return statistics."""
    print(f"Analyzing images in: {folder_path}")
    
    image_files = get_image_files(folder_path)
    
    if not image_files:
        print("No image files found in the specified folder and subfolders.")
        return
    
    print(f"Found {len(image_files)} image files")
    
    widths = []
    heights = []
    resolutions = []
    failed_files = []
    
    for image_path in image_files:
        try:
            with Image.open(image_path) as img:
                width, height = img.size
                widths.append(width)
                heights.append(height)
                resolutions.append((width, height))
                print(f"  {os.path.relpath(image_path, folder_path)}: {width}x{height}")
        except Exception as e:
            failed_files.append((image_path, str(e)))
            print(f"  Failed to process {image_path}: {e}")
    
    if not widths:
        print("No valid images could be processed.")
        return
    
    # Calculate statistics
    print("\n" + "="*60)
    print("IMAGE RESOLUTION STATISTICS")
    print("="*60)
    
    print(f"Total images processed: {len(widths)}")
    if failed_files:
        print(f"Failed to process: {len(failed_files)} files")
    
    print("\nWIDTH STATISTICS:")
    print(f"  Average width:  {statistics.mean(widths):.2f} pixels")
    print(f"  Median width:   {statistics.median(widths):.2f} pixels")
    print(f"  Min width:      {min(widths)} pixels")
    print(f"  Max width:      {max(widths)} pixels")
    print(f"  Width std dev:  {statistics.stdev(widths):.2f} pixels" if len(widths) > 1 else "  Width std dev:  N/A (single image)")
    
    print("\nHEIGHT STATISTICS:")
    print(f"  Average height: {statistics.mean(heights):.2f} pixels")
    print(f"  Median height:  {statistics.median(heights):.2f} pixels")
    print(f"  Min height:     {min(heights)} pixels")
    print(f"  Max height:     {max(heights)} pixels")
    print(f"  Height std dev: {statistics.stdev(heights):.2f} pixels" if len(heights) > 1 else "  Height std dev: N/A (single image)")
    
    # Calculate aspect ratios
    aspect_ratios = [w/h for w, h in resolutions]
    print("\nASPECT RATIO STATISTICS:")
    print(f"  Average ratio:  {statistics.mean(aspect_ratios):.3f}")
    print(f"  Median ratio:   {statistics.median(aspect_ratios):.3f}")
    print(f"  Min ratio:      {min(aspect_ratios):.3f}")
    print(f"  Max ratio:      {max(aspect_ratios):.3f}")
    
    # Common resolutions
    resolution_counts = {}
    for res in resolutions:
        resolution_counts[res] = resolution_counts.get(res, 0) + 1
    
    print("\nMOST COMMON RESOLUTIONS:")
    sorted_resolutions = sorted(resolution_counts.items(), key=lambda x: x[1], reverse=True)
    for i, ((w, h), count) in enumerate(sorted_resolutions[:5]):
        print(f"  {i+1}. {w}x{h}: {count} images")
    
    # Total pixel count statistics
    pixel_counts = [w * h for w, h in resolutions]
    print("\nTOTAL PIXEL COUNT STATISTICS:")
    print(f"  Average pixels: {statistics.mean(pixel_counts):,.0f}")
    print(f"  Median pixels:  {statistics.median(pixel_counts):,.0f}")
    print(f"  Min pixels:     {min(pixel_counts):,}")
    print(f"  Max pixels:     {max(pixel_counts):,}")
    
    if failed_files:
        print("\nFAILED FILES:")
        for file_path, error in failed_files:
            print(f"  {os.path.relpath(file_path, folder_path)}: {error}")

def main():
    """Main function to run the image analysis."""
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Usage: python bistro_helper.py <folder_path> [--lowres]")
        print("Example: python bistro_helper.py C:/path/to/images")
        print("Example: python bistro_helper.py C:/path/to/images --lowres")
        print("  --lowres: Also create a lowres copy of all images (max 128px)")
        return
    
    folder_path = sys.argv[1]
    create_lowres = len(sys.argv) == 3 and sys.argv[2] == "--lowres"
    
    if not os.path.exists(folder_path):
        print(f"Error: Folder '{folder_path}' does not exist.")
        return
    
    if not os.path.isdir(folder_path):
        print(f"Error: '{folder_path}' is not a directory.")
        return
    
    try:
        analyze_image_resolutions(folder_path)
        
        if create_lowres:
            create_lowres_copy(folder_path)
            
    except Exception as e:
        print(f"Error during analysis: {e}")

if __name__ == "__main__":
    main()