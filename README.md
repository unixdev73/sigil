# SIGIL - A matrix visualizer

This is still very much a work in progress :)

## Dependencies

- Vulkan >= 1.3 (glslangValidator, loader, validation layers)
- cfgtk (https://github.com/unixdev73/cfgtk)
- C++20
- CMake
- GLFW

## Building

```console
cmake -B build
make -C build
```

The executable should be present in the build dir under the name *sigil*

## Command-Line Options

### `--width, -w W`
Sets the window width.

### `--height, -h H`
Sets the window height.

### `--help`
Prints a help message.

### `--verbose, -v`
Increases the amount of information printed to the console.

### `--debug, -d`
Enables the KHRONOS validation layer.
