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

        # For all platforms: enable AVX instructions
        # Common flags for all platforms
        compile_flags = [
            '-O3',          # Optimization level
            '-fPIC',        # Position Independent Code
            '-mavx',        # Enable AVX instructions
            '-mavx2',       # Enable AVX2 instructions
            '-mfma'         # Enable FMA instructions (often used with AVX)
        ]

        # Determine compiler and platform-specific settings
        if IS_WINDOWS:
            # Use mingw32 on Windows
            compiler = 'gcc'  # or 'x86_64-w64-mingw32-gcc' if using specific mingw
            print("\nUsing MinGW GCC on Windows with AVX support")

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
        # Modify this section in your CustomBuildExt class to properly handle universal build

        elif IS_MACOS:
            compiler = 'gcc'  # or use 'clang' if preferred
            print("\nUsing GCC on macOS for universal build")

            # Check if GCC is available
            try:
                subprocess.run(
                    [compiler, '--version'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                print(f"{compiler} is available")
            except (subprocess.SubprocessError, FileNotFoundError):
                print(f"ERROR: {compiler} not found in PATH")
                print("Consider installing GCC with: brew install gcc")
                return

            # Check architecture and handle universal builds
            is_arm = platform.machine().startswith('arm')
            print(
                f"Detected macOS architecture: {'ARM64' if is_arm else 'x86_64'}")

            # Check for universal build environment variable
            build_universal = os.environ.get(
                'MACOS_UNIVERSAL', 'false').lower() == 'true'

            if build_universal:
                print("Building universal binary for both ARM64 and x86_64")
                # For universal binary, build two separate libraries and then combine
                arch_targets = ['arm64', 'x86_64']

                # Remove architecture-specific flags
                base_flags = [
                    flag for flag in compile_flags if not flag.startswith('-m')]

                # Temporary libraries for each architecture
                temp_libs = []

                for arch in arch_targets:
                    print(f"\nCompiling for {arch} architecture...")
                    arch_output = os.path.join(
                        output_dir, f"tinyveclib_{arch}.dylib")
                    temp_libs.append(arch_output)

                    # Architecture-specific flags
                    if arch == 'arm64':
                        arch_flags = ['-mfpu=neon', '-arch', 'arm64']
                    else:
                        arch_flags = ['-mavx', '-mavx2',
                                      '-mfma', '-arch', 'x86_64']

                    # Compile object files for this architecture
                    arch_obj_files = []
                    obj_dir = f'obj_{arch}'
                    os.makedirs(obj_dir, exist_ok=True)

                    for src in source_paths:
                        obj_file = os.path.join(
                            obj_dir, os.path.basename(src).replace('.c', '.o'))
                        arch_obj_files.append(obj_file)

                        compile_cmd = [
                            compiler, *base_flags, *arch_flags, '-c', src, '-o', obj_file
                        ]

                        print(f"Compiling for {arch}: {os.path.basename(src)}")
                        print(f"Command: {' '.join(compile_cmd)}")

                        try:
                            result = subprocess.run(compile_cmd,
                                                    check=True,
                                                    stdout=subprocess.PIPE,
                                                    stderr=subprocess.PIPE,
                                                    text=True)
                        except subprocess.CalledProcessError as e:
                            print(f"Compilation ERROR for {arch}: {e}")
                            if e.stderr:
                                print(f"STDERR: {e.stderr}")
                            return

                    # Link architecture-specific library
                    link_cmd = [
                        compiler,
                        '-shared',
                        '-dynamiclib',
                        '-o', arch_output,
                        *arch_obj_files,
                        *base_flags,
                        *arch_flags,
                        '-Wl,-install_name,@rpath/tinyveclib.dylib'
                    ]

                    print(f"Linking {arch} library: {' '.join(link_cmd)}")

                    try:
                        subprocess.run(link_cmd, check=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        print(f"Created {arch} library: {arch_output}")
                    except subprocess.CalledProcessError as e:
                        print(f"Linking ERROR for {arch}: {e}")
                        if e.stderr:
                            print(f"STDERR: {e.stderr}")
                        return

                # Create universal binary using lipo
                lib_name = "tinyveclib.dylib"
                output_path = os.path.join(output_dir, lib_name)
                lipo_cmd = ['lipo', '-create',
                            '-output', output_path, *temp_libs]

                print(f"\nCreating universal binary: {' '.join(lipo_cmd)}")
                try:
                    subprocess.run(lipo_cmd, check=True)
                    print(f"Created universal binary: {output_path}")

                    # Clean up temporary architecture-specific libraries
                    for temp_lib in temp_libs:
                        os.remove(temp_lib)

                    # Clean up object directories
                    for arch in arch_targets:
                        obj_dir = f'obj_{arch}'
                        shutil.rmtree(obj_dir, ignore_errors=True)

                except subprocess.CalledProcessError as e:
                    print(f"Error creating universal binary: {e}")
                    return
            else:
                # Single architecture build based on current machine
                if is_arm:
                    # ARM Mac (Apple Silicon)
                    print("Building for ARM64 architecture only")
                    compile_flags = [
                        flag for flag in compile_flags if not flag.startswith('-mavx')]
                    compile_flags.extend(['-mfpu=neon', '-arch', 'arm64'])
                else:
                    # Intel Mac
                    print("Building for x86_64 architecture only")
                    compile_flags.extend(['-arch', 'x86_64'])

                lib_name = "tinyveclib.dylib"
        else:
            # Linux or other Unix
            compiler = 'gcc'
            print("\nUsing GCC on Linux/Unix with AVX support")

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

        dummy_c_path = os.path.join(os.path.dirname(
            os.path.abspath(__file__)), "src", "dummy.c")

        os.makedirs(os.path.dirname(dummy_c_path), exist_ok=True)
        if not os.path.exists(dummy_c_path):
            with open(dummy_c_path, "w") as f:
                f.write("/* Dummy file for extension */\n")
                f.write(
                    "PyMODINIT_FUNC PyInit__dummy(void) { return NULL; }\n")


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


dummy_extension = Extension(
    "tinyvec._dummy",
    sources=["src/dummy.c"],  # Create this empty file
    optional=True  # Makes it not required to build
)

setup(
    name="tinyvec",
    version="0.1.0",
    description="A tiny vector database",
    author="Your Name",
    author_email="your.email@example.com",
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
