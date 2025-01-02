# DBT-RISE-EVE

A Instruction Set Simulator based on DBT-RISE implementing the EveCore Instruction Set.

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
3. Use python to create a venv: `python3 -m venv .venv && source .venv/bin/activate`
4. Install conan and cmake: `pip install conan cmake`
5. Build the project using the CMakePresets: `cmake --preset Debug && cmake --build build/Debug -j`

## Usage

When building, two executables are created: `eve_sim_interp` and `eve_sim_tcc`. Both of them are a standalone ISS, a executable in hex format can be loaded using the `--file` option.
For an overview use the `--help` option.

## Features

- ISS Implementation of EveCore using a Harvard Architecture
- A very simple ISS, showcasing two different simulation techniques
- A showcase of CoreDSL in the `coredsl` branch 

## License

```
This project is licensed under the MIT License - see the LICENSE file for details.
```

## Acknowledgments

- Harry H. Porter III, Ph.D. for his Open and Free EveCore Specification and Implementation.

## Contact Information

In case you have any questions, remarks or want to try out CoreDSL, feel free to reach out to me at alex@minres.com.
