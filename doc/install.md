# Install

## Make Targets
- `main`: This builds a swSIM executable.
- `main-dbg`: This builds a debug swSIM binary with debug information and the address sanitizer enabled.
- `clean`: Performs a cleanup of the project and all sub-modules.

### Make Variables

#### `ARG`
The `ARG` variable is appended to the default compiler flags. To use it, just append the `ARG` variable when calling `make`, e.g., ```make main-dbg ARG="-DDEBUG_CLR"```.

All possible values that can be added to `ARG` are:
- `-DDEUBG_CLR` to add color to the printed debug logs.

#### `ARG_SWICC`
This variable is passed as the `ARG` variable when compiling swICC. Take a look at swICC documentation to find out how it's used, and all possible values that can be passed.
