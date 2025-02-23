# SIGIL - A matrix visualizer

This program is meant to help visualize matrices in 3D<br>
A path is traced from the smallest element in the matrix to the largest one.<br>
The largest element in the matrix will be placed the deepest along the Z axis.
To not do this, use the *compress* option.

To rotate the sigil around the X axis, press the X key and the up / down
arrow key. The same goes for the Y, and Z axes. <br>

To translate the sigil about the X axis, press the X key and the left / right
arrow key. The same goes for the Y, and Z axes. <br>

To zoom in / out on the sigil, press the = / - key.<br>
To reset the transformations press R.

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

### `--party, -p
Takes a number of milliseconds that are waited
before the sigil changes colors.

### `--file, -f
Specifies the file that holds the matrix.

### `--compress, -c
Assign zero depth to each vertex.

### `--red, -r
Takes a value between 0 and 255 and sets the red color component of the sigil.

### `--green, -g
Takes a value between 0 and 255 and sets the green color component of the sigil.

### `--blue, -b
Takes a value between 0 and 255 and sets the blue color component of the sigil.

## Examples

The sample matrices in the *data* directory are copied to the build dir.
To visualize the 3x3 matrix in random colors, type:

```console
./build/sigil --file ./mat3.txt --party 500
```
