from glob import glob
from itertools import zip_longest
import os
import sys

from config import Config
from project import build_bytecode_vm, project_root, run_program


def main():
    # Run all tests in release
    build_bytecode_vm(Config.Release)

    fails = 0
    passes = 0
    omits = 0

    samples_dir = os.path.join(project_root(), 'samples')
    for file in glob(os.path.join(samples_dir, '*.lox')):
        expected_file = file.replace('.lox', '.expected')
        if not os.path.exists(expected_file):
            print(f"[OMIT] {file} ... no '.expected' output file")
            omits += 1
            continue

        with open(expected_file, 'r') as expected_file:
            expected_output = expected_file.read()
        real_output = run_program(file, Config.Release)

        if real_output is None:
            real_output = ""
        real_lines = real_output.splitlines()
        expected_lines = expected_output.splitlines()
        merged = list(zip_longest(real_lines, expected_lines, fillvalue=""))
        passed = all([left == right for left, right in merged])

        if passed:
            print(f'[PASS] {file}')
            passes += 1
        else:
            print(f'[FAIL] {file}')
            for (real, expected) in merged:
                if real == expected:
                    print(f'    {real}')
                else:
                    print(f'    {real} <=> {expected}')
            fails += 1

    print(f"Passed: {passes}")
    print(f"Failed: {fails}")
    print(f"Omits:  {omits}")
    print(f"Total:  {passes + fails + omits}")

    if fails > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
