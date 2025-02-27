from setuptools import setup, find_packages
from setuptools.command.build_ext import build_ext
from setuptools.command.install import install
from setuptools.command.bdist_egg import bdist_egg
from setuptools.command.sdist import sdist
from wheel.bdist_wheel import bdist_wheel

import subprocess
import platform
import os


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
            compiler = 'gcc'
            print("\nUsing MinGW GCC on Windows with AVX support")

            # Add AVX instructions for Windows
            compile_flags.extend([
                '-mavx',        # Enable AVX instructions
                '-mavx2',       # Enable AVX2 instructions
                '-mfma'         # Enable FMA instructions
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
            compiler = 'gcc'
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
                '-mfma'         # Enable FMA instructions
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


# Custom sdist to ensure the shared library is included in the source distribution
class CustomSdist(sdist):
    def run(self):
        # Create a comprehensive MANIFEST.in file
        setup_dir = os.path.dirname(os.path.abspath(__file__))
        manifest_dir = os.path.join(setup_dir, "MANIFEST.in")

        with open(manifest_dir, "w") as f:
            # C source files
            f.write("recursive-include src/core/src *.c *.h\n")
            f.write("recursive-include src/core/include *.h\n")

            # Build scripts / configuration files
            f.write("include setup.py\n")
            f.write("include README*\n")
            f.write("include LICENSE*\n")

            # Don't include compiled binary files in source dist
            f.write("exclude src/tinyvec/core/*.dll\n")
            f.write("exclude src/tinyvec/core/*.so\n")
            f.write("exclude src/tinyvec/core/*.dylib\n")

        print(f"Created comprehensive MANIFEST.in file for source distribution")

        super().run()

# Custom wheel to ensure the shared library is included in the wheel


class CustomBdistWheel(bdist_wheel):
    def run(self):
        self.run_command('build_ext')
        super().run()

    def finalize_options(self):
        super().finalize_options()
        # Mark the wheel as platform-specific
        self.root_is_pure = False

    def get_tag(self):
        # Override the tag generation
        python, abi, plat = super().get_tag()

        # Keep the platform tag for Windows but set Python and ABI tags to 'any'
        if IS_WINDOWS:
            python = 'py3'
            abi = 'none'
            # plat remains the same (win_amd64 or win32)

        return python, abi, plat


# def get_package_data_files():
#     data_files = []

#     # Add shared library files based on platform
#     if IS_WINDOWS:
#         data_files.append(
#             ('tinyvec/core', ['src/tinyvec/core/tinyveclib.dll']))
#     elif IS_MACOS:
#         data_files.append(
#             ('tinyvec/core', ['src/tinyvec/core/tinyveclib.dylib']))
#     else:
#         data_files.append(('tinyvec/core', ['src/tinyvec/core/tinyveclib.so']))

#     return data_files


setup(
    name="tinyvecdb",
    version="0.1.3",
    description="TinyVecDB is a high performance, lightweight, embedded vector database for similarity search.",
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
    include_package_data=True,
    zip_safe=False,
)
