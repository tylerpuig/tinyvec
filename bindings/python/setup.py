from setuptools import setup
from setuptools.command.build_ext import build_ext
import subprocess
import platform
import os

IS_MACOS = platform.system() == "Darwin"
IS_WINDOWS = platform.system() == "Windows"


class CustomBuildExt(build_ext):
    def run(self):
        # Manually compile and link the shared library
        setup_dir = os.path.dirname(os.path.abspath(__file__))

        sources = ['db.c', 'minheap.c', 'distance.c', 'file.c', 'cJSON.c']
        source_paths = [os.path.join(
            setup_dir, 'src', 'core', 'src', src) for src in sources]

        if IS_WINDOWS:
            compiler = 'cl'  # Use MSVC
            # Optimizations & DLL build
            compile_flags = ['/O2', '/LD', '/MD', '/DVERSION_INFO=\\"1.0\\"']
            link_flags = ['/link', '/DLL']
        else:
            # Other platforms use GCC/Clang style flags
            compiler = 'clang' if IS_MACOS else 'gcc'
            compile_flags = ['-O3']
            if IS_MACOS:
                # Check if on ARM architecture (Apple Silicon)
                if platform.machine().startswith('arm'):
                    compile_flags = [
                        '-mfpu=neon',  # Neon SIMD instructions for ARM
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

            # Use the same compilation approach for all platforms
            cmd = [
                compiler, *compile_flags, '-c', src, '-o', obj_file
            ]
            subprocess.run(cmd, check=True)

        # Build shared library
        if IS_WINDOWS:
            lib_name = "tinyveclib.dll"
            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            cmd = [
                compiler, '-shared', *compile_flags,
                '-o', output_path, *obj_files
            ]
        elif IS_MACOS:
            lib_name = "tinyveclib.dylib"
            link_cmd = [compiler, '-shared', '-dynamiclib',
                        '-undefined', 'dynamic_lookup']
            link_flags = compile_flags
            link_flags.append('-Wl,-install_name,@rpath/' + lib_name)

            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            cmd = [
                *link_cmd, *link_flags, *obj_files, '-o', output_path
            ]

            subprocess.run(cmd, check=True)
        else:
            # Linux or other Unix
            lib_name = "tinyveclib.so"
            link_cmd = [compiler, '-shared']
            link_flags = compile_flags

            output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, lib_name)

            cmd = [
                *link_cmd, *link_flags, *obj_files, '-o', output_path
            ]

            subprocess.run(cmd, check=True)

        # Clean up object files
        for obj in obj_files:
            try:
                os.remove(obj)
                print(f"Cleaned up: {obj}")
            except OSError as e:
                print(f"Failed to clean up {obj}: {e}")

        try:
            os.rmdir('obj')

        except OSError as e:
            print(f"Failed to remove obj directory: {e}")


setup(
    cmdclass={
        'build_ext': CustomBuildExt,
    },
    package_dir={"": "src"},
    packages=["tinyvec", "tinyvec.core"],
    package_data={
        "tinyvec.core": ["*.dll", "*.so", "*.dylib"],
    },
    zip_safe=False,
    include_package_data=True,
)
