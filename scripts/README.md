# Scripts

Singular `.lox` program files can be run, as well as a set of `.lox` programs
and their expected outputs.

## Running a `.lox` file

This will build the bytecode virtual machine and run the associated file:

Example:

```
python .\scripts\run_sample.py .\samples\expressions.lox --config Release
```

## Acceptance Test Battery

Acceptance tests associate a .lox file with a .expected file in the `samples/`
directory. They assume a .lox file produces a standard output representation
of that given by the .expected file. Acceptance tests are run in Release mode.

Run the test battery like:

```
python .\scripts\run_acceptance_test.py
```
