# build.py
import subprocess
import platform
import os


def build():
    sources = ['db.c', 'minheap.c', 'distance.c',  'file.c', 'cJSON.c']
    source_paths = [f'../../src/core/src/{src}' for src in sources]

    # Compile object files
    for src in source_paths:
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
    subprocess.run([
        *link_cmd, '-O3', '-march=native', '-mavx2',
        '-ffast-math', '-fopenmp', *obj_files,
        '-o', f'src/tinyvec/core/{lib_name}'
    ], check=True)

    # Clean up object files
    for obj in obj_files:
        os.remove(obj)


if __name__ == "__main__":
    build()
