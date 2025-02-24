from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import platform
import os

IS_MACOS = platform.system() == "Darwin"


class CustomBuildExt(build_ext):
    def run(self):
        print("="*50)
        print("Starting C library build process")
        print("="*50)

        setup_dir = os.path.dirname(os.path.abspath(__file__))
        print(f"Setup directory: {setup_dir}")

        sources = ['db.c', 'minheap.c', 'distance.c', 'file.c', 'cJSON.c']
        source_paths = [os.path.join(
            setup_dir, 'src', 'core', 'src', src) for src in sources]

        print("Source paths:", source_paths)

        compiler = 'clang' if IS_MACOS else 'gcc'
        compile_flags = ['-O3']

        if not IS_MACOS:
            # Add these flags only for non-macOS systems
            compile_flags.extend(['-march=native', '-mavx2', '-ffast-math'])

        # Compile object files
        for src in source_paths:
            print(f"Compiling {src}")
            subprocess.run([
                compiler, *compile_flags, '-c', src
            ], check=True)

        # Build shared library
        if platform.system() == "Windows":
            lib_name = "tinyveclib.dll"
            link_cmd = [compiler, '-shared']
        elif platform.system() == "Darwin":
            lib_name = "tinyveclib.dylib"
            link_cmd = [compiler, '-shared', '-dynamiclib',
                        '-undefined', 'dynamic_lookup']
        else:
            lib_name = "tinyveclib.so"
            link_cmd = [compiler, '-shared']

        obj_files = [f.replace('.c', '.o') for f in sources]
        output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, lib_name)

        link_flags = compile_flags
        if IS_MACOS:
            link_flags.append('-Wl,-install_name,@rpath/' + lib_name)

        subprocess.run([
            *link_cmd, *link_flags, *obj_files,
            '-o', output_path
        ], check=True)

        # Clean up object files
        for obj in obj_files:
            try:
                os.remove(obj)
                print(f"Cleaned up: {obj}")
            except OSError as e:
                print(f"Failed to clean up {obj}: {e}")


compile_args = ['-O3']
link_args = []

if not IS_MACOS:
    compile_args.extend(['-march=native', '-mavx2', '-ffast-math'])

ext_module = Extension(
    "tinyvec.core.tinyveclib",
    sources=[
        'src/core/src/db.c',
        'src/core/src/minheap.c',
        'src/core/src/distance.c',
        'src/core/src/file.c',
        'src/core/src/cJSON.c'
    ],
    extra_compile_args=compile_args,
    extra_link_args=link_args
)

setup(
    name="tinyvec",
    version="0.1.0",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    ext_modules=[ext_module],
    include_package_data=True,
    package_data={
        "tinyvec.core": ["*.dll", "*.so", "*.dylib", "tinyveclib.dll"],
    },
    install_requires=[
        "numpy",
        "setuptools>=75.8.0",
        "wheel>=0.45.1"
    ],
    cmdclass={
        'build_ext': CustomBuildExt,
    },
    author="tylerpuig",
    description="Tiny vector database",
    python_requires=">=3.7",
)
