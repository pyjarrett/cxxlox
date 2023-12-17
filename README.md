# C++ Lox Bytecode Virtual Machine

A C++ implementation of the bytecode virtual machine from "Crafting Interpreters"
by Robert Nystrom.

# Philosophy

This uses some elements of the C++ standard library, but tries to stay close
to the philosophy of "doing things from scratch" that the book uses.

For example, that means I use `<iostream>` and `std::format`, but wrote a
simple resizable array type instead of using `std::vector`.  It also means
using C-style tagged unions instead of `std::variant`.

I have never written a bytecode VM before, so I'm doing it as
close to the book in a more type-safe C++ way, and then refactoring into
more idiomatic C++ as I go.  This won't ever be a production bytecode
VM, but it should eventually show my C++ version of how the book
does things.

# Building

This project currently uses `cmake`, which you can build as follows.

Use an out-of-tree build:

```
mkdir build
cd build
cmake ..         # Generate project
cmake --build .  # Build project
ctest            # Run unit tests
```

# Running a `.lox` file

This will build the bytecode virtual machine and run the associated file:

Example:

```
python .\scripts\run_sample.py .\samples\expressions.lox --config Release
```

# Acceptance Test Battery

Acceptance tests compare the standard output of running the VM on a file ending
in `.lox`, against the expected output in a a similarly named `.expected` file.

The acceptance test script builds the VM and then runs the tests
in Release mode.

Run the test battery like:

```
python .\scripts\run_acceptance_test.py
```
