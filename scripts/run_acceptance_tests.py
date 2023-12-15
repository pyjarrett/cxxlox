from glob import glob
import os
import subprocess

from config import Config
from project import project_root, run_program


def main():
    samples_dir = os.path.join(project_root(), 'samples')
    for file in glob(os.path.join(samples_dir, '*.lox')):
        print(file)

        expected_file = file.replace('.lox', '.expected')
        if not os.path.exists(expected_file):
            # print("...No expected file")
            continue

        expected_output = ''
        with open(expected_file, 'r') as expected_file:
            expected_output = expected_file.read()
        real_output = run_program(file, Config.Release)

        real_lines = real_output.splitlines()
        expected_lines = expected_output.splitlines()
        merged = zip(real_lines, expected_lines)
        passed = [left == right for left, right in merged]

        if passed:
            print(f'[PASS] {file}')
        if not passed:
            for real, expected in merged:
                print(f'{real} <=> {expected}')
            print(f'[FAIL] {file}')

        # If failed, rerun in debug.


if __name__ == "__main__":
    main()
