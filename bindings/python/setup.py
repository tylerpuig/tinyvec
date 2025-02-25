from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
from setuptools.command.install import install
from setuptools.command.bdist_egg import bdist_egg
from setuptools.command.sdist import sdist
from wheel.bdist_wheel import bdist_wheel

import subprocess
import platform
import os
import sys
import shutil
from pathlib import Path

IS_MACOS = platform.system() == "Darwin"
IS_WINDOWS = platform.system() == "Windows"


class CustomBuildExt(build_ext):
    def run(self):
        print("\n" + "="*80)
        print("CUSTOM BUILD EXTENSION RUNNING WITH GCC + AVX SUPPORT")
        print("="*80)

        # Manually compile and link the shared library
        setup_dir = os.path.dirname(os.path.abspath(__file__))
        print(f"Setup directory: {setup_dir}")

        sources = ['db.c', 'minheap.c', 'distance.c', 'file.c', 'cJSON.c']
        source_paths = [os.path.join(
            setup_dir, 'src', 'core', 'src', src) for src in sources]

        # Verify source files exist
        for src in source_paths:
            if not os.path.exists(src):
                print(f"ERROR: Source file does not exist: {src}")
                return
            else:
                print(f"Found source file: {src}")

        # Create a directory for object files
        os.makedirs('obj', exist_ok=True)
        print("Created obj directory")

        # Create output directory
        output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
        os.makedirs(output_dir, exist_ok=True)
        print(f"Output directory: {output_dir}")

        # Common flags for all platforms
        compile_flags = [
            '-O3',          # Optimization level
            '-fPIC',        # Position Independent Code
        ]

        # Determine compiler and platform-specific settings
        if IS_WINDOWS:
            # Use mingw32 on Windows
            compiler = 'gcc'  # or 'x86_64-w64-mingw32-gcc' if using specific mingw
            print("\nUsing MinGW GCC on Windows with AVX support")

            # Add AVX instructions for Windows
            compile_flags.extend([
                '-mavx',        # Enable AVX instructions
                '-mavx2',       # Enable AVX2 instructions
                '-mfma'         # Enable FMA instructions (often used with AVX)
            ])

            # Check if GCC is available
            try:
                subprocess.run(
                    [compiler, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                print(f"{compiler} is available")
            except (subprocess.SubprocessError, FileNotFoundError):
                print(f"ERROR: {compiler} not found in PATH")
                print("Make sure you have MinGW installed and in your PATH")
                return

            lib_name = "tinyveclib.dll"

        elif IS_MACOS:
            compiler = 'gcc'  # or use 'clang' if preferred
            print("\nUsing GCC on macOS")

            # Check if GCC is available
            try:
                subprocess.run(
                    [compiler, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                print(f"{compiler} is available")
            except (subprocess.SubprocessError, FileNotFoundError):
                # Try with clang as fallback on macOS
                compiler = 'clang'
                try:
                    subprocess.run(
                        [compiler, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                    print(f"Using {compiler} instead")
                except (subprocess.SubprocessError, FileNotFoundError):
                    print(f"ERROR: Neither gcc nor clang found in PATH")
                    print("Consider installing GCC with: brew install gcc")
                    return

            # Check architecture and set appropriate flags
            is_arm = platform.machine().startswith('arm')
            if is_arm:
                # ARM Mac (Apple Silicon)
                print("Building for ARM64 architecture")
                compile_flags.extend(['-arch', 'arm64'])
            else:
                # Intel Mac
                print("Building for x86_64 architecture")
                compile_flags.extend([
                    '-mavx',
                    '-mavx2',
                    '-mfma',
                    '-arch', 'x86_64'
                ])

            lib_name = "tinyveclib.dylib"
        else:
            # Linux or other Unix
            compiler = 'gcc'
            print("\nUsing GCC on Linux/Unix with AVX support")

            # Add AVX instructions for Linux
            compile_flags.extend([
                '-mavx',        # Enable AVX instructions
                '-mavx2',       # Enable AVX2 instructions
                '-mfma'         # Enable FMA instructions (often used with AVX)
            ])

            # Check if GCC is available
            try:
                subprocess.run(
                    [compiler, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                print(f"{compiler} is available")
            except (subprocess.SubprocessError, FileNotFoundError):
                print(f"ERROR: {compiler} not found in PATH")
                print("Install GCC with your package manager")
                return

            lib_name = "tinyveclib.so"

        # Compile object files
        obj_files = []
        for src in source_paths:
            obj_file = os.path.join(
                'obj', os.path.basename(src).replace('.c', '.o'))
            obj_files.append(obj_file)

            compile_cmd = [
                compiler, *compile_flags, '-c', src, '-o', obj_file
            ]

            print(f"\nCompiling: {os.path.basename(src)}")
            print(f"Command: {' '.join(compile_cmd)}")

            try:
                result = subprocess.run(compile_cmd,
                                        check=True,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE,
                                        text=True)
                if result.stdout:
                    print(f"Compilation output: {result.stdout}")

                # Verify object file was created
                if os.path.exists(obj_file):
                    print(f"Created object file: {obj_file}")
                else:
                    print(f"ERROR: Failed to create object file: {obj_file}")
                    return

            except subprocess.CalledProcessError as e:
                print(f"Compilation ERROR: {e}")
                if e.stdout:
                    print(f"STDOUT: {e.stdout}")
                if e.stderr:
                    print(f"STDERR: {e.stderr}")
                return

        # Link the shared library
        output_path = os.path.join(output_dir, lib_name)

        print("\n" + "-"*40)
        print(f"LINKING SHARED LIBRARY: {output_path}")
        print("-"*40)

        if IS_WINDOWS:
            # MinGW linking on Windows
            link_cmd = [
                compiler,
                '-shared',
                '-o', output_path,
                *obj_files,
                *compile_flags
            ]
        elif IS_MACOS:
            # macOS linking
            link_cmd = [
                compiler,
                '-shared',
                '-dynamiclib',
                '-o', output_path,
                *obj_files,
                *compile_flags,
                '-Wl,-install_name,@rpath/' + lib_name
            ]
        else:
            # Linux linking
            link_cmd = [
                compiler,
                '-shared',
                '-o', output_path,
                *obj_files,
                *compile_flags
            ]

        print(f"Link command: {' '.join(link_cmd)}")

        try:
            result = subprocess.run(link_cmd,
                                    check=True,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE,
                                    text=True)
            if result.stdout:
                print(f"Linking output: {result.stdout}")

            # Verify library was created
            if os.path.exists(output_path):
                print(f"SUCCESS: Created shared library: {output_path}")
                print(f"Library size: {os.path.getsize(output_path)} bytes")

                # Create a manifest file to ensure package data is included
                manifest_dir = os.path.join(setup_dir, "MANIFEST.in")
                with open(manifest_dir, "w") as f:
                    f.write(f"include src/tinyvec/core/{lib_name}\n")
                print(f"Created MANIFEST.in file")
            else:
                print(f"ERROR: Failed to create shared library: {output_path}")
                return

        except subprocess.CalledProcessError as e:
            print(f"Linking ERROR: {e}")
            if e.stdout:
                print(f"STDOUT: {e.stdout}")
            if e.stderr:
                print(f"STDERR: {e.stderr}")
            return

        # Clean up object files
        print("\nCleaning up...")
        for obj in obj_files:
            try:
                if os.path.exists(obj):
                    os.remove(obj)
                    print(f"Cleaned up: {obj}")
            except OSError as e:
                print(f"Failed to clean up {obj}: {e}")

        try:
            if os.path.exists('obj') and len(os.listdir('obj')) == 0:
                os.rmdir('obj')
                print("Removed obj directory")
        except OSError as e:
            print(f"Failed to remove obj directory: {e}")

        print("\nCustomBuildExt completed")

        # Still need to create dummy.c for setuptools to use as a placeholder extension
        # dummy_c_path = os.path.join(os.path.dirname(
        #     os.path.abspath(__file__)), "src", "dummy.c")

        # os.makedirs(os.path.dirname(dummy_c_path), exist_ok=True)
        # if not os.path.exists(dummy_c_path):
        #     with open(dummy_c_path, "w") as f:
        #         f.write("/* Dummy file for extension */\n")
        #         f.write(
        #             "PyMODINIT_FUNC PyInit__dummy(void) { return NULL; }\n")


# Custom sdist to ensure the shared library is included in the source distribution
class CustomSdist(sdist):
    def run(self):
        self.run_command('build_ext')
        super().run()


# Custom wheel to ensure the shared library is included in the wheel
class CustomBdistWheel(bdist_wheel):
    def run(self):
        self.run_command('build_ext')
        super().run()


# Get package data files
def get_package_data_files():
    data_files = []

    # Add the shared library files based on platform
    if IS_WINDOWS:
        data_files.append(
            ('tinyvec/core', ['src/tinyvec/core/tinyveclib.dll']))
    elif IS_MACOS:
        data_files.append(
            ('tinyvec/core', ['src/tinyvec/core/tinyveclib.dylib']))
    else:
        data_files.append(('tinyvec/core', ['src/tinyvec/core/tinyveclib.so']))

    return data_files


setup(
    name="tinyvec",
    version="0.1.0",
    description="A tiny vector database",
    cmdclass={
        'build_ext': CustomBuildExt,
        'sdist': CustomSdist,
        'bdist_wheel': CustomBdistWheel,
    },
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    package_data={
        "tinyvec.core": ["*.dll", "*.so", "*.dylib"],
    },
    data_files=get_package_data_files(),
    include_package_data=True,
    zip_safe=False,
)
