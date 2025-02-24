from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import platform
import os
import sys

IS_MACOS = platform.system() == "Darwin"
IS_WINDOWS = platform.system() == "Windows"


class CustomBuildExt(build_ext):
    def run(self):
        # Manually compile and link our shared library

        setup_dir = os.path.dirname(os.path.abspath(__file__))
        print(f"Setup directory: {setup_dir}")

        sources = ['db.c', 'minheap.c', 'distance.c', 'file.c', 'cJSON.c']
        source_paths = [os.path.join(
            setup_dir, 'src', 'core', 'src', src) for src in sources]

        # Use appropriate compiler and flags based on platform
        if IS_WINDOWS:
            # For Windows, use MSVC with appropriate flags
            compiler = 'cl'
            compile_flags = [
                '/O2',           # Optimization level (equivalent to -O2)
                '/arch:AVX',     # Enable AVX instructions
                '/arch:AVX2',    # Enable AVX2 instructions
                # Fast floating point model (similar to -ffast-math)
                '/fp:fast',
                '/GL',           # Whole program optimization
                '/Gy',           # Function-level linking
                '/Oi',           # Enable intrinsic functions
                '/Ot'            # Favor fast code
            ]

            # Additional link flags for Windows
            link_flags = [
                '/LTCG'          # Link-time code generation
            ]

        else:
            # Other platforms use GCC/Clang style flags
            compiler = 'clang' if IS_MACOS else 'gcc'
            compile_flags = ['-O3']
            if IS_MACOS:
                # Check if we're on ARM architecture (Apple Silicon)
                if platform.machine().startswith('arm'):
                    compile_flags = [
                        '-mfpu=neon',  # Enable Neon SIMD instructions for ARM
                        '-O3',
                        '-mtune=native'
                    ]
                else:
                    # Intel-based Mac
                    compile_flags = [
                        '-O3',
                        '-mtune=native',
                        '-mavx',  # Intel SIMD instructions
                        '-mavx2'
                    ]

        # Create a directory for object files
        os.makedirs('obj', exist_ok=True)

        # Compile object files
        obj_files = []
        for src in source_paths:
            obj_file = os.path.join(
                'obj', os.path.basename(src).replace('.c', '.o'))
            obj_files.append(obj_file)
            print(f"Compiling {src} to {obj_file}")

            if IS_WINDOWS:
                # Windows compilation with MSVC
                subprocess.run([
                    'cl', '/c', '/nologo', *
                    compile_flags, src, f'/Fo{obj_file}'
                ], check=True)
            else:
                # Unix compilation
                subprocess.run([
                    compiler, *compile_flags, '-c', src, '-o', obj_file
                ], check=True)

        # Build shared library
        if IS_WINDOWS:
            lib_name = "tinyveclib.dll"
            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            # For Windows, use link.exe
            print(f"Linking to {output_path}")
            subprocess.run([
                'link', '/DLL', '/nologo', '/OUT:' + output_path, *obj_files
            ], check=True)
        elif IS_MACOS:
            lib_name = "tinyveclib.dylib"
            link_cmd = [compiler, '-shared', '-dynamiclib',
                        '-undefined', 'dynamic_lookup']
            link_flags = compile_flags
            link_flags.append('-Wl,-install_name,@rpath/' + lib_name)

            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            print(f"Linking to {output_path}")
            subprocess.run([
                *link_cmd, *link_flags, *obj_files, '-o', output_path
            ], check=True)
        else:
            # Linux or other Unix
            lib_name = "tinyveclib.so"
            link_cmd = [compiler, '-shared']
            link_flags = compile_flags

            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            print(f"Linking to {output_path}")
            subprocess.run([
                *link_cmd, *link_flags, *obj_files, '-o', output_path
            ], check=True)

        # Clean up object files
        for obj in obj_files:
            try:
                os.remove(obj)
                print(f"Cleaned up: {obj}")
            except OSError as e:
                print(f"Failed to clean up {obj}: {e}")

        try:
            os.rmdir('obj')
            print("Removed obj directory")
        except OSError as e:
            print(f"Failed to remove obj directory: {e}")


# We're building the library manually and not using setuptools Extension
setup(
    cmdclass={
        'build_ext': CustomBuildExt,
    },
)
