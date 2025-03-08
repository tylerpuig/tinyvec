import os
import shutil
import sys


def copy_dir(src, dest):
    """Recursively copy a directory."""
    os.makedirs(dest, exist_ok=True)

    for item in os.listdir(src):
        src_path = os.path.join(src, item)
        dest_path = os.path.join(dest, item)

        if os.path.isdir(src_path):
            # Recursively copy directories
            copy_dir(src_path, dest_path)
        else:
            # Copy files
            shutil.copy2(src_path, dest_path)


def copy_dependencies():
    try:
        # Get the equivalent of __dirname in Python
        current_dir = os.path.dirname(os.path.abspath(__file__))

        # Calculate paths
        root_dir = os.path.abspath(os.path.join(current_dir, "../../../../"))
        core_src = os.path.join(root_dir, "tinyvec/src/core")
        target_dir = os.path.abspath(os.path.join(current_dir, ".."))

        # Copy core include files
        copy_dir(
            os.path.join(core_src, "include"),
            os.path.join(target_dir, "src/core/include")
        )

        # Copy core source files
        copy_dir(
            os.path.join(core_src, "src"),
            os.path.join(target_dir, "src/core/src")
        )

        print("Successfully copied core files")
    except Exception as e:
        print(f"Error copying dependencies: {e}", file=sys.stderr)
        sys.exit(1)


# if __name__ == "__main__":
#     copy_dependencies()
