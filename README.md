# DBT-RISE-EVE

An Instruction Set Simulator based on DBT-RISE implementing the EveCore Instruction Set.

## Description

This project is a showcase of the DBT-RISE Library, implementing a simple Instruction Set. It offers three (partial) implementations and some very basic example programs. Notably two different Simulation approaches are shown, a simple Interpeter based and one using Just-In-Time Compilation.

The EveCore is a Microcontroller Architecture created to be a very simple (but still usefull) Microcontroller. Documentation, a System Verilog Implementation and more can be found [here](https://blitz64.org/EveCore/).

This repository consists of two main branches:
- The 'manual' branch consists of a minimial handwritten implementation of the interp and tcc Simulation engine. It only implements some of the EveCore Instructions.
- The "coredsl' branch showcases the approach of using a higher level description of the core to quickly implement new instructions or alter the core. It also provides a more complete instructions set and some more features.

## Prerequisites

This project uses `cmake` and `conan` as build tools. Python can be used for setup.

## Installation

Step-by-step instructions on how to set up the project:
1. Clone the repository: `git clone <repository-url>`
2. Navigate to the project directory: `cd <project-directory>`
3. Checkout the branch: `git switch coredsl`
4. Use python to create a venv: `python3 -m venv .venv && source .venv/bin/activate`
5. Install conan and cmake: `pip install conan cmake`
6. Build the project using the CMakePresets: `cmake --preset Debug && cmake --build build/Debug -j`

## Usage

When building, the executable `eve_sim_interp` is created. For an overview of all available options use the `--help` flag. The instruction count is by default limited to 100 (as no HALT instruction or similar is implemented), this can be changed with the appropriate option.

## Features

- Easily extendable and configurable ISS Implementation of EveCore
- High level and easily understandable description of the core and its instructions in the `.core_desc` file

## License

```
This project is licensed under the MIT License - see the LICENSE file for details.
```

## Acknowledgments

- Harry H. Porter III, Ph.D. for his Open and Free EveCore Specification and Implementation.

## Contact Information

In case you have any questions, remarks or want to know more about CoreDSL, feel free to reach out to me at alex@minres.com.
