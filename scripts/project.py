from pathlib import Path
import os
import subprocess

# Local scripts
from config import Config


def project_root() -> Path:
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


def cmake_build_root(config: Config) -> str:
    return f"build-{config.name.lower()}"


def build_bytecode_vm(config: Config) -> bool:
    root_dir: Path = project_root()
    previous_dir = os.getcwd()
    os.chdir(root_dir)

    # Make the build directory if needed.
    build_dir: str = cmake_build_root(config)
    if not os.path.exists(build_dir):
        os.mkdir(build_dir)
    os.chdir(build_dir)

    # The build directory might exist, but CMake might not have been run yet.
    if not os.path.exists("CMakeCache.txt"):
        result: subprocess.CompletedProcess = subprocess.run(f"cmake -DCMAKE_BUILD_TYPE={config.name} ..".split())
        if result.returncode != 0:
            print("Failed to generate CMake project.")
            return False

    # The true "build" step.
    # --config is only needed in multi-config generations, like Visual Studio
    result = subprocess.run(f"cmake --build . -j32 --config={config.name}".split())
    if result.returncode != 0:
        print("Building the VM failed.")
        return False

    # Verify the project unit tests pass.
    result = subprocess.run(["ctest", "-C", config.name, "--build-and-test"])
    if result.returncode != 0:
        print("Compiler and/or VM unit tests failed.")
        return False

    os.chdir(previous_dir)
    return True


def vm_path(config: Config) -> Path:
    exe_name: str = "cxxlox_vm_cli.exe"
    return project_root().joinpath(cmake_build_root(config), "src", config.name, exe_name)


def run_program(program_name: Path, config: Config) -> str:
    vm = vm_path(config)
    try:
        return subprocess.check_output([vm, program_name]).decode('utf-8')
    except subprocess.CalledProcessError as cpe:
        print(f"Failed to run vm {vm} on {program_name}")
        print(cpe)
        return ""
    except FileNotFoundError as fnfe:
        print(f"File not found to run vm {vm} on {program_name}")
        print(fnfe)
        return ""
    except Exception as ex:
        print(f"An unexpected error occurred while running vm {vm} on {program_name}")
        print(ex)
        return ""
