# Electric Piano

This repository contains the hardware and software sources for a custom electric piano project built around the ATmega16 microcontroller.

## Project Structure

The project is divided into the following main directories:

- **circuit/**: Contains the hardware design files.
  - `eagle/`: Schematics and PCB layouts designed in EAGLE.
  - `gerber/`: Manufacturing files for the PCB.
  - `parts.txt`: Bill of materials (BOM).
  - `stencil.svg`: Stencil design for solder paste application.

- **firmware/**: The C++ firmware running on the ATmega16 (16MHz).
  - `firmware.cpp`: Main application code.
  - `bootloader.cpp`: Custom bootloader.
  - `runfile`: Build and flash instructions using `avr-g++` and `avrdude` via STK500v1 over serial.

- **programer/**: A custom programmer utility written in Rust.

## Prerequisites

The easiest way to get started is by using the provided Nix flake. If you have [Nix](https://nixos.org/) installed, simply run:

```bash
nix develop
```

This will drop you into a development shell with all the required dependencies (`avr-gcc`, `avrdude`, `cargo`, `just`, etc.) installed.

If you are not using Nix, you will need to manually install:
- `avr-gcc` / `avr-g++`
- `avr-libc`
- `avrdude`
- Rust toolchain (`cargo`)
- [`just`](https://github.com/casey/just) command runner

## Building and Flashing

This project uses `just` as a command runner. You can see all available commands by running:

```bash
just --list
```

From the root directory, you can run:

- Compile the bootloader: `just bootloader`
- Compile the firmware: `just firmware`
- Flash the bootloader: `just flash-bootloader`
- Write fuses: `just write-fuses`
- Build the Rust programmer: `just build-programmer`
- Run the Rust programmer: `just run-programmer`
- Build everything: `just build-all`
