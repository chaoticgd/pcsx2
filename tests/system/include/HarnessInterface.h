// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

/// This file contains definitions for the ABI that the part of harness
/// implemented as a guest program uses to communicate with the part of the
/// harness implemented as a host program.
namespace HarnessInterface
{
	/// The number of the fake system call number that allows the guest part of
	/// the test harness to cooperate with the host part.
	static constexpr int SYSTEM_CALL_NUMBER = 0xbd;

	static constexpr int REGISTER_OPCODE = 4; // a0
	static constexpr int REGISTER_PARAMETER_LIST = 5; // a1
	static constexpr int REGISTER_PARAMETER_COUNT = 6; // a2

	enum class Opcode
	{
		TEST_BEGIN,
		TEST_END,
		ASSERT_TRUE,
		ASSERT_FALSE,
		ASSERT_S64,
		ASSERT_U64,
		ASSERT_FLOAT,
		ASSERT_STRING,
	};

	// TEST_*
	static constexpr int PARAMETER_TEST_CATEGORY = 0;
	static constexpr int PARAMETER_TEST_NAME = 1;
	static constexpr int PARAMETER_TEST_EXPECTED_ASSERTION_COUNT = 2;

	// ASSERT_*
	enum class Comparison
	{
		EQUAL,
		NOT_EQUAL,
		LESS_THAN,
		LESS_THAN_OR_EQUAL,
		GREATER_THAN,
		GREATER_THAN_OR_EQUAL,
		EQUAL_IGNORING_CASE,
		NOT_EQUAL_IGNORING_CASE
	};

	static constexpr int PAREMETER_COMPARISON = 0;
	static constexpr int PARAMETER_VALUE_0 = 1;
	static constexpr int PARAMETER_VALUE_1 = 2;
	static constexpr int PARAMETER_STRING_0 = 3;
	static constexpr int PARAMETER_STRING_1 = 4;
	static constexpr int PARAMETER_SOURCE_FILE = 5;
	static constexpr int PARAMETER_LINE_NUMBER = 6;
} // namespace HarnessInterface
