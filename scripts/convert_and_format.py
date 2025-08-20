#!/usr/bin/env python3
"""
Script to convert specific file types to UTF-8 with BOM and apply .clang-format
Usage: python convert_and_format.py <directory_path>
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path


def has_bom(file_path):
    """Check if file has UTF-8 BOM"""
    try:
        with open(file_path, 'rb') as f:
            return f.read(3) == b'\xef\xbb\xbf'
    except Exception:
        return False


def convert_to_utf8_bom(file_path):
    """Convert file to UTF-8 with BOM (codepage 65001)"""
    try:
        # Read the file content with various encodings
        content = None
        encodings_to_try = ['utf-8', 'utf-8-sig', 'cp1252', 'latin1', 'cp949', 'cp932']
        
        for encoding in encodings_to_try:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    content = f.read()
                print(f"  Successfully read with encoding: {encoding}")
                break
            except UnicodeDecodeError:
                continue
        
        if content is None:
            print(f"  ERROR: Could not decode file with any encoding")
            return False
        
        # Write back with UTF-8 BOM
        with open(file_path, 'w', encoding='utf-8-sig') as f:
            f.write(content)
        
        print(f"  Converted to UTF-8 with BOM")
        return True
        
    except Exception as e:
        print(f"  ERROR: {e}")
        return False


def apply_clang_format(file_path):
    """Apply clang-format to the file"""
    try:
        # First check if clang-format is available
        result = subprocess.run(['clang-format', '--version'], 
                              capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  WARNING: clang-format not found in PATH")
            return False
        
        # Apply clang-format in-place
        result = subprocess.run(['clang-format', '-i', str(file_path)], 
                              capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"  Applied clang-format")
            return True
        else:
            print(f"  ERROR: clang-format failed: {result.stderr}")
            return False
            
    except FileNotFoundError:
        print(f"  WARNING: clang-format not found")
        return False
    except Exception as e:
        print(f"  ERROR: {e}")
        return False


def process_file(file_path):
    """Process a single file: convert to UTF-8 BOM and apply clang-format"""
    print(f"Processing: {file_path}")
    
    # Check if already has BOM
    if has_bom(file_path):
        print(f"  Already has UTF-8 BOM")
    else:
        if not convert_to_utf8_bom(file_path):
            return False
    
    # Apply clang-format
    apply_clang_format(file_path)
    
    return True


def is_in_excluded_folder(file_path, excluded_folders):
    """Check if file is inside any of the excluded folders"""
    file_path = Path(file_path)
    for excluded in excluded_folders:
        try:
            # Check if any parent directory matches the excluded folder name
            for parent in file_path.parents:
                if parent.name.lower() == excluded.lower():
                    return True
        except Exception:
            continue
    return False


def find_and_process_files(directory, extensions):
    """Find and process all files with specified extensions"""
    directory = Path(directory)
    
    if not directory.exists():
        print(f"ERROR: Directory '{directory}' does not exist")
        return
    
    if not directory.is_dir():
        print(f"ERROR: '{directory}' is not a directory")
        return
    
    # Define target extensions
    target_extensions = {'.h', '.hpp', '.cpp'} # 주의: 셰이더 파일들은 BOM이 추가되면 제대로 읽지 못함
    
    # Define folders to exclude
    excluded_folders = ['vcpkg_installed', 'x64']
    
    # Find all files with target extensions, excluding specified folders
    files_found = []
    for ext in target_extensions:
        for file_path in directory.rglob(f'*{ext}'):
            if not is_in_excluded_folder(file_path, excluded_folders):
                files_found.append(file_path)
    
    if not files_found:
        print(f"No files with target extensions found in '{directory}' (excluding {', '.join(excluded_folders)})")
        return
    
    print(f"Found {len(files_found)} files to process")
    print(f"Target extensions: {', '.join(sorted(target_extensions))}")
    print(f"Excluded folders: {', '.join(excluded_folders)}")
    print("-" * 60)
    
    processed = 0
    failed = 0
    
    for file_path in sorted(files_found):
        try:
            if process_file(file_path):
                processed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"ERROR processing {file_path}: {e}")
            failed += 1
        print()
    
    print("-" * 60)
    print(f"Summary:")
    print(f"  Processed successfully: {processed}")
    print(f"  Failed: {failed}")
    print(f"  Total: {len(files_found)}")


def create_default_clang_format():
    """Create a default .clang-format file if it doesn't exist"""
    clang_format_content = """---
# .clang-format configuration for C++14 projects
BasedOnStyle: Microsoft
Language: Cpp
Standard: Cpp14

# Indentation
IndentWidth: 4
TabWidth: 4
UseTab: Never
ContinuationIndentWidth: 4

# Alignment
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignEscapedNewlines: Left
AlignOperands: true
AlignTrailingComments: true

# Breaking
AllowShortBlocksOnASingleLine: false
AllowShortCaseLabelsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
AlwaysBreakAfterReturnType: None
AlwaysBreakBeforeMultilineStrings: false
AlwaysBreakTemplateDeclarations: true
BreakBeforeBinaryOperators: None
BreakBeforeBraces: Allman
BreakBeforeTernaryOperators: true
BreakConstructorInitializers: BeforeColon
BreakInheritanceList: BeforeColon
BreakStringLiterals: true

# Spacing
SpaceAfterCStyleCast: false
SpaceAfterLogicalNot: false
SpaceAfterTemplateKeyword: true
SpaceBeforeAssignmentOperators: true
SpaceBeforeCtorInitializerColon: true
SpaceBeforeInheritanceColon: true
SpaceBeforeParens: ControlStatements
SpaceBeforeRangeBasedForLoopColon: true
SpaceInEmptyParentheses: false
SpacesBeforeTrailingComments: 1
SpacesInAngles: false
SpacesInCStyleCastParentheses: false
SpacesInContainerLiterals: true
SpacesInParentheses: false
SpacesInSquareBrackets: false

# Other
ColumnLimit: 100
MaxEmptyLinesToKeep: 1
NamespaceIndentation: None
PointerAlignment: Left
ReflowComments: true
SortIncludes: true
SortUsingDeclarations: true
"""
    
    clang_format_path = Path('.clang-format')
    
    if not clang_format_path.exists():
        print("Creating default .clang-format file...")
        with open(clang_format_path, 'w', encoding='utf-8') as f:
            f.write(clang_format_content)
        print(f"Created: {clang_format_path.absolute()}")
    else:
        print(f"Using existing .clang-format file: {clang_format_path.absolute()}")


def main():
    parser = argparse.ArgumentParser(
        description='Convert C++ files to UTF-8 with BOM and apply clang-format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
File types processed:
  .h, .hpp    - C++ header files
  .cpp        - C++ source files  
  .frag       - Fragment shader files
  .vert       - Vertex shader files
  .comp       - Compute shader files

The script will:
1. Convert all target files to UTF-8 with BOM (codepage 65001)
2. Apply clang-format formatting
3. Create a default .clang-format file if none exists

Excluded folders: vcpkg_installed

Example:
  python convert_and_format.py .
  python convert_and_format.py C:\\MyProject\\src
        """
    )
    
    parser.add_argument('directory', 
                       help='Directory to process (will search subdirectories)')
    parser.add_argument('--create-config', action='store_true',
                       help='Only create .clang-format config file and exit')
    
    args = parser.parse_args()
    
    if args.create_config:
        create_default_clang_format()
        return
    
    print("File Converter and Formatter")
    print("=" * 60)
    print(f"Target directory: {Path(args.directory).absolute()}")
    print()
    
    # Create default clang-format if needed
    create_default_clang_format()
    print()
    
    # Process files
    find_and_process_files(args.directory, ['.h', '.hpp', '.cpp', '.frag', '.vert', '.comp'])


if __name__ == '__main__':
    main()