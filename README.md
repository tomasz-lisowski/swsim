# swSIM
swSIM is an all-software SIM card. It's the first publicly available (to the best of my knowledge) SIM card simulator which does not rely on any SIM hardware to work.

In summary:
- A software-only SIM card simulator.
- It does **NOT** depend on any hardware to work.
- Can attach to the PC through a [software-only PC/SC reader](https://github.com/tomasz-lisowski/swicc-drv-ifd) and show up as a PC/SC card.
- The PC/SC interface allows it to connect to **ANY** phone with a SIM card slot and e.g. the [SIMtrace 2](https://osmocom.org/projects/simtrace2/wiki) device running on the [cardem firmware](https://osmocom.org/projects/simtrace2/wiki#card-emulation) or any other tool which forwards messages to and from the phone.

## Building
Make sure to have `make` and `gcc` installed. Also make sure that you clone the repository recursively so that all sub-modules get cloned as expected.
The make targets are as follows:
- **main**: This builds a swSIM executable.
- **main-dbg**: This builds a debug swSIM binary with debug information and the address sanitizer enabled.
- **clean**: Performs a cleanup of the project and all sub-modules.

## Instructions
Take a look at the usage message. You will need a file system definition (examples present in `/data`) or a `.swicc` file system file before running. swSIM is a client so make sure to start a swICC server beforehand (like that in the [PC/SC reader for swSIM](https://github.com/tomasz-lisowski/swicc-drv-ifd)).
