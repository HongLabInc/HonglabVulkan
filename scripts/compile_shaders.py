import os
import time
import subprocess
import sys
from pathlib import Path
from datetime import datetime
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class ShaderCompilerHandler(FileSystemEventHandler):
    def __init__(self, watch_dir, output_dir=None):
        self.watch_dir = Path(watch_dir)
        self.output_dir = Path(output_dir) if output_dir else self.watch_dir
        
        # Supported shader file extensions
        self.shader_extensions = {
            '.vert': 'vertex',
            '.frag': 'fragment', 
            '.geom': 'geometry',
            '.tesc': 'tessellation control',
            '.tese': 'tessellation evaluation',
            '.comp': 'compute'
        }
        
        # Cache glslc path to avoid repeated PATH lookups
        self.glslc_path = self._find_glslc()
        
        print(f"Monitoring shader directory: {self.watch_dir}")
        print(f"Output directory: {self.output_dir}")
        print(f"Supported extensions: {list(self.shader_extensions.keys())}")
        print("-" * 50)

    def _find_glslc(self):
        """Find and cache glslc executable path"""
        try:
            result = subprocess.run(['where', 'glslc'] if os.name == 'nt' else ['which', 'glslc'], 
                                   capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                return result.stdout.strip().split('\n')[0]  # Get first match
        except:
            pass
        return 'glslc'  # Fallback to PATH lookup

    def on_modified(self, event):
        if event.is_directory:
            return
            
        file_path = Path(event.src_path)
        
        # Check if it's a shader file we care about
        if file_path.suffix in self.shader_extensions:
            self.compile_shader(file_path)

    def compile_shader(self, shader_path):
        """Compile a single shader file to SPV"""
        try:
            # Generate output path (same name with .spv extension)
            relative_path = shader_path.relative_to(self.watch_dir)
            output_path = self.output_dir / f"{relative_path}.spv"
            
            # Skip compilation if output is newer than source
            if (output_path.exists() and 
                output_path.stat().st_mtime > shader_path.stat().st_mtime):
                return
            
            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Build glslc command with cached path
            cmd = [
                self.glslc_path,
                str(shader_path),
                '-o', str(output_path)
            ]
            
            # Generate timestamp only when needed
            timestamp = datetime.now().strftime("%H:%M:%S")
            print(f"[{timestamp}] Compiling: {shader_path.name} -> {output_path.name}")
            
            # Run glslc with minimal overhead
            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True,
                cwd=self.watch_dir,
                timeout=30  # Prevent hanging
            )
            
            if result.returncode == 0:
                print(f"[{timestamp}] ✓ Success: {output_path.name}")
                # Only print output if it exists and is non-empty
                if result.stdout.strip():
                    print(f"  Output: {result.stdout.strip()}")
            else:
                print(f"[{timestamp}] ✗ Error compiling {shader_path.name}:")
                # Combine stderr and stdout for efficiency
                error_output = (result.stderr or '') + (result.stdout or '')
                if error_output.strip():
                    print(f"  Error: {error_output.strip()}")
                    
        except subprocess.TimeoutExpired:
            timestamp = datetime.now().strftime("%H:%M:%S")
            print(f"[{timestamp}] ✗ Timeout compiling {shader_path.name}")
        except Exception as e:
            timestamp = datetime.now().strftime("%H:%M:%S")
            print(f"[{timestamp}] ✗ Unexpected error: {e}")

    def compile_all_shaders(self):
        """Compile all existing shader files in the directory"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Compiling all existing shaders...")
        
        # Use generator for memory efficiency
        shader_files = []
        for ext in self.shader_extensions.keys():
            shader_files.extend(self.watch_dir.rglob(f"*{ext}"))
        
        if not shader_files:
            print("No shader files found.")
            return
            
        compiled_count = 0
        for shader_file in shader_files:
            # Check if compilation is needed before calling compile_shader
            relative_path = shader_file.relative_to(self.watch_dir)
            output_path = self.output_dir / f"{relative_path}.spv"
            
            if (not output_path.exists() or 
                output_path.stat().st_mtime <= shader_file.stat().st_mtime):
                self.compile_shader(shader_file)
                compiled_count += 1
        
        print(f"[{datetime.now().strftime('%H:%M:%S')}] Initial compilation complete. "
              f"Compiled {compiled_count}/{len(shader_files)} shader files.")
        print("-" * 50)

def main():
    # Default to current directory if no argument provided
    if len(sys.argv) > 1:
        watch_directory = sys.argv[1]
    else:
        # Look for shaders directory relative to script location
        script_dir = Path(__file__).parent
        watch_directory = script_dir / "shaders"
        
        # If shaders directory doesn't exist, use current directory
        if not watch_directory.exists():
            watch_directory = Path.cwd()
    
    watch_directory = Path(watch_directory).resolve()
    
    if not watch_directory.exists():
        print(f"Error: Directory '{watch_directory}' does not exist.")
        sys.exit(1)
    
    # Check if glslc is available (do this once)
    try:
        result = subprocess.run(['glslc', '--version'], 
                               capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            print(f"Found glslc: {result.stdout.strip()}")
        else:
            print("Warning: glslc found but version check failed")
    except (FileNotFoundError, subprocess.TimeoutExpired):
        print("Error: glslc not found or not responding. Please ensure it's installed and in your PATH.")
        sys.exit(1)
    
    # Create event handler and observer
    event_handler = ShaderCompilerHandler(watch_directory)
    observer = Observer()
    observer.schedule(event_handler, str(watch_directory), recursive=True)
    
    # Compile all existing shaders first
    event_handler.compile_all_shaders()
    
    # Start watching for changes
    observer.start()
    print("Watching for shader file changes... (Press Ctrl+C to stop)")
    
    try:
        while True:
            time.sleep(2)  # Increased sleep interval to reduce CPU usage
    except KeyboardInterrupt:
        print("\nStopping shader watcher...")
        observer.stop()
    
    observer.join()
    print("Shader watcher stopped.")

if __name__ == "__main__":
    main()