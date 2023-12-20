import argparse
from pathlib import Path
import sys

from config import Config
from project import build_bytecode_vm, run_program


def main():
    """
    Builds the bytecode vm and then runs a sample program.
    """
    parser = argparse.ArgumentParser(description="Builds the bytecode vm and then runs a sample program.")
    parser.add_argument('program_path', metavar='program_path', type=str)
    parser.add_argument('--config', default=Config.Debug.name, choices=[Config.Debug.name, Config.Release.name],
                        help='Configuration type')
    args = parser.parse_args()

    program_name: Path = Path(args.program_path)
    config: Config = Config[args.config]

    if not build_bytecode_vm(config):
        print("Failed to build the virtual machine.")
        sys.exit(1)

    print(run_program(program_name, config))


if __name__ == "__main__":
    main()
