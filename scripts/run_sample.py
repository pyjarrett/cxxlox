import os
from pathlib import Path
import subprocess
import sys


def build_root() -> Path:
    """
    Looks upward for a build root where a .git resides.

    This allows finding the build root from other directories, such as to when
    scripts from inside the scripts/ directory.
    """
    current_dir: Path = Path(os.curdir).absolute()
    while not current_dir.joinpath(".git").exists():
        if current_dir.parent == current_dir:
            raise RuntimeError(
                "Could not find root directory.  No more parent paths to look up."
            )
        current_dir = current_dir.parent
    return current_dir


def build_bytecode_vm(root_dir: Path):
    previous_dir = os.getcwd()
    os.chdir(root_dir)

    # Make the build directory if needed.
    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")

    # The buidl directory might exist, but CMake might not have been run yet.
    if not os.path.exists("CMakeCache.txt"):
        subprocess.run("cmake ..".split())

    # The true "build" step.
    subprocess.run("cmake --build . -j32".split())
    os.chdir(previous_dir)


def vm_path() -> Path:
    return build_root().joinpath("build", "src", "Debug", "cxxlox_vm_cli.exe")


def run_program(program_name: Path) -> str:
    vm = vm_path()
    print(f"Running program: {program_name}")
    print(f"Interpreter {vm}")
    return subprocess.check_output([vm, program_name])


def main():
    """
    Builds the bytecode vm and then runs a sample program.
    """
    if len(sys.argv) < 2:
        print("Expected a file name to run.")
        print(f"Usage {sys.argv[0]} [filename.lox]")
        sys.exit(1)

    program_name: str = Path(sys.argv[1])

    root_dir: Path = build_root()
    print(f"Build root is {root_dir}")

    build_bytecode_vm(root_dir)

    # TODO: Allow running under different program configurations: debug, release, etc.
    print(run_program(program_name).decode("utf-8"))


if __name__ == "__main__":
    main()
