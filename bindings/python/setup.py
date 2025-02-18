from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import platform
import os


class CustomBuildExt(build_ext):
    def run(self):
        print("="*50)
        print("Starting C library build process")
        print("="*50)

        setup_dir = os.path.dirname(os.path.abspath(__file__))
        print(f"Setup directory: {setup_dir}")

        sources = ['db.c', 'minheap.c', 'distance.c', 'file.c', 'cJSON.c']
        source_paths = [os.path.join(
            setup_dir, '../../src/core/src', src) for src in sources]

        print("Source paths:", source_paths)

        # Compile object files
        for src in source_paths:
            print(f"Compiling {src}")
            subprocess.run([
                'gcc', '-O3', '-march=native', '-mavx2',
                '-ffast-math', '-fopenmp', '-c', src
            ], check=True)

        # Build shared library
        if platform.system() == "Windows":
            lib_name = "tinyveclib.dll"
            link_cmd = ['gcc', '-shared']
        elif platform.system() == "Darwin":
            lib_name = "tinyveclib.dylib"
            link_cmd = ['gcc', '-shared', '-dynamiclib']
        else:
            lib_name = "tinyveclib.so"
            link_cmd = ['gcc', '-shared']

        obj_files = [f.replace('.c', '.o') for f in sources]
        output_dir = os.path.join(setup_dir, 'src', 'tinyvec', 'core')
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, lib_name)

        print(f"Building shared library: {output_path}")
        subprocess.run([
            *link_cmd, '-O3', '-march=native', '-mavx2',
            '-ffast-math', '-fopenmp', *obj_files,
            '-o', output_path
        ], check=True)

        print(f"Built library at: {output_path}")

        # Clean up object files
        for obj in obj_files:
            try:
                os.remove(obj)
                print(f"Cleaned up: {obj}")
            except OSError as e:
                print(f"Failed to clean up {obj}: {e}")

        # Don't call build_ext.run(self) as we're not using actual Extension objects


setup(
    name="tinyvec",
    version="0.1.0",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    package_data={
        "tinyvec.core": ["*.dll", "*.so", "*.dylib"],
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
