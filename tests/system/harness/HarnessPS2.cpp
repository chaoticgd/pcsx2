// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Test.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// This is the part of the test harness that runs as a PS2 program. It can
// either run independently on real hardware, or it can run cooperatively with
// a host harness.

// Whether we're running under a host test harness or independently.
static _Bool cooperative_mode = false;

int main(int argc, char** argv)
{
	cooperative_mode = (argc == 2 && strcmp(argv[1], "--cooperative") == 0);

	Test::SortTests();
	Test::RunAll();

	fflush(stdout);
	fflush(stderr);
}

void Test::Register(Test* test)
{
	if (s_tail == nullptr)
	{
		s_head = test;
		s_tail = test;
	}
	else
	{
		s_tail->m_next = test;
		test->m_prev = s_tail;
		s_tail = test;
	}
}

void Test::RunAll()
{
	for (Test* test = s_head; test != nullptr; test = test->m_next)
	{
		TestInterface::BeginTest(test->Category(), test->Name());
		try
		{
			test->Run();
		}
		catch (const AssertionFailed& ex)
		{
			printf("Test aborted");
		}
		TestInterface::EndTest(test->Category(), test->Name());
	}
}

Test* Test::s_head = nullptr;
Test* Test::s_tail = nullptr;

void TestInterface::BeginTest(const char* category, const char* name)
{
}

void TestInterface::EndTest(const char* category, const char* name)
{
}

void TestInterface::AssertTrue(bool value, const char* string, const AssertionParams& params)
{
}

void TestInterface::AssertFalse(bool value, const char* string, const AssertionParams& params)
{
}

void TestInterface::AssertS64(IntegerComparison op, s64 values[2], const char* strings[2], const AssertionParams& params)
{
}

void TestInterface::AssertU64(IntegerComparison op, u64 values[2], const char* strings[2], const AssertionParams& params)
{
}

void TestInterface::AssertString(StringComparison op, const char* values[2], const char* strings[2], const AssertionParams& params)
{
}

void TestInterface::AssertFloat(FloatComparison op, const float values[2], const char* strings[2], const AssertionParams& params)
{
}
