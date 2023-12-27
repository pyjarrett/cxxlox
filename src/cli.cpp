#include "cli.hpp"

#include "chunk.hpp"
#include "common.hpp"
#include "debug.hpp"
#include "vm.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace cxxlox {

void repl()
{
	std::cout << "Lox REPL.  'exit' or 'quit' to stop.\n";

	// Loop
	while (true) {
		// Read
		std::string line;
		std::cout << " > ";
		if (!std::getline(std::cin, line)) {
			break;
		}

		if (line == "exit" || line == "quit") {
			break;
		}

		// Evaluate
		CL_UNUSED(interpret(line));
	}
}

[[nodiscard]] bool readFile(const char* fileName, std::string& out)
{
	if (!fileName) {
		return false;
	}

	// Open file at the end to get the length and then read it all in one go.
	std::ifstream file(fileName, std::ios::in | std::ios::ate);

	if (!file) {
		return false;
	}

	const auto numBytes = file.tellg();
	file.seekg(std::ios_base::beg);
	out.reserve(size_t(numBytes));
	out.assign(std::istreambuf_iterator<char>(file), {});
	return true;
}

void runFile(const char* fileName)
{
	std::string source;
	if (!readFile(fileName, source)) {
		std::cerr << "Unable to open file '" << fileName << "'\n";
		exit(ExitCodeIOError);
	}

	const InterpretResult result = interpret(source);
	if (result == InterpretResult::RuntimeError) {
		exit(ExitCodeInternalSoftwareError);
	}
	if (result == InterpretResult::CompileError) {
		exit(ExitCodeDataFormatError);
	}

	VM::instance().reset();
}

int runMain(int argc, char** argv)
{
	// Running as a REPL, since the first arg is this program's name.
	if (argc == 1) {
		repl();
	} else if (argc == 2) {
		// Running a file.
		runFile(argv[1]);
	} else {
		std::cout << "Usage: cxxlox [filename]\n";
		return ExitCodeBadUsage;
	}

	return ExitCodeOk;
}

} // namespace cxxlox