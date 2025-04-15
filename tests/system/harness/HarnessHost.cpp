// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// This is the part of the test harness that runs on the host. It records the
// test results, makes sure that assertions aren't incorrectly skipped, and
// compares said results to previous results stored in a file.

#include "HarnessInterface.h"

#include "pcsx2/BuildVersion.h"
#include "pcsx2/VMManager.h"

#include "common/CrashHandler.h"
#include "common/Threading.h"

#include <cstdlib>

enum class HarnessFlags
{
	VERBOSE = 1 << 0
};

struct HarnessOptions
{
	std::vector<std::string> test_results_compare_paths;
	std::optional<std::string> test_results_output_path;
	bool verbose = false;
};

static std::unique_ptr<const HarnessOptions> s_harness_options;

static Threading::Thread s_cpu_thread;

static std::unique_ptr<HarnessOptions> ParseCommandLineArguments(int argc, char** argv);
static void PrintUsageMessage();
static void StartCPUThread();

int main(int argc, char** argv)
{
	CrashHandler::Install();

#ifndef _WIN32
	const char* error;
	if (!VMManager::PerformEarlyHardwareChecks(&error))
	{
		std::fprintf(stderr, "%s\n", error);
		return EXIT_FAILURE;
	}
#endif

	s_harness_options = ParseCommandLineArguments(argc, argv);
	if (!s_harness_options)
		return EXIT_FAILURE;

	s_cpu_thread.SetStackSize(VMManager::EMU_THREAD_STACK_SIZE);
	s_cpu_thread.Start(StartCPUThread);

	return EXIT_SUCCESS;
}

static std::unique_ptr<HarnessOptions> ParseCommandLineArguments(int argc, char** argv)
{
	std::unique_ptr<HarnessOptions> options = std::make_unique<HarnessOptions>();
	for (int i = 1; i < argc; i++)
	{
		char* argument = argv[i];

		if (std::strcmp(argument, "--compare") == 0)
		{
		}
		else if (std::strcmp(argument, "--output") == 0)
		{
			i++;
			if (i >= argc)
			{
				std::fprintf(stderr, "Error: Expected file path after '--output'.\n\n");
				PrintUsageMessage();
				return nullptr;
			}

			if (options->test_results_output_path)
			{
				std::fprintf(stderr, "Error: Multiple output paths specified.\n\n");
				PrintUsageMessage();
				return nullptr;
			}
		}
		else if (std::strcmp(argument, "--help") == 0)
		{
			PrintUsageMessage();
			return nullptr;
		}
	}

	return options;
}

static void PrintUsageMessage()
{
	std::fprintf(stderr, "PCSX2 System Test Harness %s\n", BuildVersion::GitRev);
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

static void StartCPUThread()
{
}
