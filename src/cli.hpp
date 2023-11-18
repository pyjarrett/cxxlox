#pragma once

namespace cxxlox {

// Linux style exit codes.
// https://man7.org/linux/man-pages/man3/sysexits.h.3head.html
enum ExitCode
{
	ExitCodeOk = 0,
	ExitCodeBadUsage = 64,
	ExitCodeDataFormatError = 65,
	ExitCodeInternalSoftwareError = 70,
	ExitCodeIOError = 74,
};

int runMain(int argc, char** argv);

} // namespace cxxlox
