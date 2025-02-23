import fs from "fs/promises";
import path from "path";

const ROOT_DIR = path.resolve(__dirname, "../../../../");
const CORE_SRC = path.join(ROOT_DIR, "tinyvec/src/core");
const TARGET_DIR = path.join(__dirname, "..");

async function copyDir(src: string, dest: string) {
  // Create destination directory
  await fs.mkdir(dest, { recursive: true });

  // Read source directory
  const entries = await fs.readdir(src, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);

    if (entry.isDirectory()) {
      // Recursively copy directories
      await copyDir(srcPath, destPath);
    } else {
      // Copy files
      await fs.copyFile(srcPath, destPath);
    }
  }
}

async function copyDependencies() {
  try {
    // Copy core include files
    await copyDir(
      path.join(CORE_SRC, "include"),
      path.join(TARGET_DIR, "src/core/include")
    );

    // Copy core source files
    await copyDir(
      path.join(CORE_SRC, "src"),
      path.join(TARGET_DIR, "src/core/src")
    );

    console.log("Successfully copied core files");
  } catch (err) {
    console.error("Error copying dependencies:", err);
    process.exit(1);
  }
}

copyDependencies();
