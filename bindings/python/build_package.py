#!/usr/bin/env python3
"""
Helper script to build the tinyvec package
"""
import os
import sys
import subprocess
import shutil


def main():
    # Check if the build directory exists, if so, remove it
    if os.path.exists('build'):
        print("Removing existing build directory...")
        shutil.rmtree('build')

    # Check if the dist directory exists, if so, remove it
    if os.path.exists('dist'):
        print("Removing existing dist directory...")
        shutil.rmtree('dist')

    # Run the build command - don't try to install the build package again
    print("Running build command...")
    try:
        # Use python -m build instead of importing build
        subprocess.run([sys.executable, '-m', 'build',
                       '--no-isolation'], check=True)
    except subprocess.CalledProcessError:
        print("Build failed")
        return 1

    print("Build completed successfully!")
    print("Output files can be found in the 'dist' directory.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
