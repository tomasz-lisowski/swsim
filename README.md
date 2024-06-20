# swSIM

> Project **needs** to be cloned recursively. Downloading the ZIP is not enough.

swSIM is an all-software SIM card. It's the first publicly available (to the best of my knowledge) SIM card simulator which does not rely on any SIM hardware to work.

## Scope
- A software-only SIM card simulator.
- It does **NOT** depend on any hardware to work.
- Can connect to the PC via PC/SC using the [swICC PC/SC reader](https://github.com/tomasz-lisowski/swicc-pcsc).
- The PC/SC interface allows it to connect to **ANY** phone with a SIM card slot. We used the [SIMtrace 2](https://osmocom.org/projects/simtrace2/wiki) device running on the [cardem firmware](https://osmocom.org/projects/simtrace2/wiki#card-emulation) but any other tool which forwards messages to and from the phone would work as well.

## Install
- You need `make` and `gcc` to compile the project. No extra runtime dependencies.
1. `sudo apt-get install make gcc`
2. `git clone --recurse-submodules git@github.com:tomasz-lisowski/swsim.git`
3. `cd swsim`
4. `make main-dbg` (for more info on building, take a look at `./doc/install.md`).

## Usage
1. Start a swICC card server, e.g., [swICC PC/SC reader](https://github.com/tomasz-lisowski/swicc-pcsc).
2. `./build/swsim.elf --ip 127.0.0.1 --port 37324 --fs filesystem.swiccfs --fs-gen ./data/usim.json`
3. `pcsc_scan` (part of the `pcsc-tools` package) will show some details of the card.
4. You can interact with the card as you would with a real card attached to a hardware card reader.
