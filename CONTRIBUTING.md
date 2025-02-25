# Contributing to TinyVec

Thank you for your interest in contributing to TinyVec! This document provides guidelines and instructions to help you get started.

## Code of Conduct

Please be respectful and considerate of others when contributing to this project.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone** the forked repository to your local machine
3. **Install dependencies** for the specific binding you're working with
4. **Create a branch** for your changes

## Development Process

### Setting Up Development Environment

#### Prerequisites:

- GCC compiler installed on your system
- Node.js 16 or higher for the JavaScript/TypeScript binding
- Python 3.7+ for the Python binding

#### For C Core Engine:

The C core engine source files are located in the `/src` directory in the main repository. Any changes to the core functionality should be made here.

#### For JavaScript/TypeScript Binding:

```bash
# Navigate to the Node.js binding directory
cd bindings/nodejs

# Install dependencies
npm install

# Copy C core files to Node.js binding directory and build the native addon
npm run build:dev

# Build TypeScript files
npm run build
```

**Important workflow notes:**

- Always edit the C source files in the main `/src` directory
- Use `npm run build:dev` to copy the updated core files to the Node.js binding and rebuild the native addon
- The `build:dev` script handles both copying the C files and rebuilding the Node.js addon
- Add tests for any new functionality in the `/tests` directory
- Ensure all tests pass by running `npm run test` before submitting a pull request

#### For Python Binding:

```bash
# Navigate to the Python binding directory
cd bindings/python

# Install development dependencies
pip install -e ".[dev]"

# After making changes to C core files in the main /src directory, copy them to the Python binding
python copy_core_deps.py

# Build the extension modules
python setup.py build_ext

# Build distribution packages (wheel and tarball)
python -m build
```

**Important workflow notes:**

- Always make changes to C files in the main `/src/core` directory
- Use `python copy_core_deps.py` to copy the updated core files to the Python binding
- Add tests for any new functionality in the `tests/` directory
- Ensure all tests pass by running `pytest` before submitting a pull request

### Making Changes

1. Make sure to write tests for your changes
2. Ensure the code style matches the project's conventions
3. Run the existing test suite to verify your changes don't break anything

```bash
# For JavaScript/TypeScript
cd bindings/nodejs
npm test

# For Python
cd bindings/python
pytest
```

## Pull Request Process

1. Update documentation to reflect any changes
2. Include a clear description of the changes in your PR
3. Link any related issues in your PR description
4. Make sure all tests pass before submitting
5. Wait for code review and address any feedback

## Reporting Issues

When reporting issues, please include:

- A clear description of the problem
- Steps to reproduce
- Expected vs. actual behavior
- Version information (OS, language version, library version)

## License

By contributing to TinyVec, you agree that your contributions will be licensed under the project's MIT license.
