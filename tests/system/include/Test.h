// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <tamtypes.h>

struct AssertionParams
{
	const char* file;
	u32 line;
	bool fatal;
};

enum class IntegerComparison
{
	EQUAL,
	NOT_EQUAL,
	LESS_THAN,
	LESS_THAN_OR_EQUAL,
	GREATER_THAN,
	GREATER_THAN_OR_EQUAL
};

template <typename T>
inline bool CompareIntegers(IntegerComparison op, T values[2])
{
	switch (op)
	{
		case IntegerComparison::EQUAL:
			return values[0] == values[1];
		case IntegerComparison::NOT_EQUAL:
			return values[0] != values[1];
		case IntegerComparison::LESS_THAN:
			return values[0] < values[1];
		case IntegerComparison::LESS_THAN_OR_EQUAL:
			return values[0] <= values[1];
		case IntegerComparison::GREATER_THAN:
			return values[0] > values[1];
		case IntegerComparison::GREATER_THAN_OR_EQUAL:
			return values[0] >= values[1];
	}

	return false;
}

enum class StringComparison : s32
{
	EQUAL,
	NOT_EQUAL,
	EQUAL_IGNORING_CASE,
	NOT_EQUAL_IGNORING_CASE
};

enum class FloatComparison : s32
{
	EQUAL,
	NOT_EQUAL
};

namespace TestInterface
{
	void BeginTest(const char* category, const char* name);
	void EndTest(const char* category, const char* name);

	void AssertTrue(bool value, const char* string, const AssertionParams& params);
	void AssertFalse(bool value, const char* string, const AssertionParams& params);
	void AssertS64(IntegerComparison op, s64 values[2], const char* strings[2], const AssertionParams& params);
	void AssertU64(IntegerComparison op, u64 values[2], const char* strings[2], const AssertionParams& params);
	void AssertString(StringComparison op, const char* values[2], const char* strings[2], const AssertionParams& params);
	void AssertFloat(FloatComparison op, const float values[2], const char* strings[2], const AssertionParams& params);
}; // namespace TestInterface

#define _ASSERT_PARAMS(f) {.file = __FILE__, .line = __LINE__, .fatal = f}
#define ASSERT_TRUE(expr) TestInterface::AssertTrue((expr), #expr, _ASSERT_PARAMS(true))
#define ASSERT_FALSE(expr) TestInterface::AssertFalse((expr), #expr, _ASSERT_PARAMS(true))

#undef _ASSERT_PARAMS

// Forward invocations of TEST() to TEST2() and TEST3() depending on the number
// of arguments. See: https://stackoverflow.com/a/11763277
#define GET_MACRO(_1, _2, _3, name, ...) name
#define TEST(...) GET_MACRO(__VA_ARGS__, TEST3, TEST2)(__VA_ARGS__)

class Test
{
public:
	virtual const char* Category() const = 0;
	virtual const char* Name() const = 0;
	virtual int GetExpectedAssertionCount() const = 0;

	virtual void Run() const = 0;

	static void Register(Test* test);
	static void SortTests();
	static void RunAll();

private:
	Test* m_prev = nullptr;
	Test* m_next = nullptr;

	static Test* s_head;
	static Test* s_tail;
};

// Create a global variable for the test so that it can register itself when its
// global constructor is called.
#define TEST2(category, name) \
	class Test_##category##_##name : public Test \
	{ \
	public: \
		Test_##category##_##name() { Register(this); } \
		const char* Category() const override { return #category; } \
		const char* Name() const override { return #name; } \
		int GetExpectedAssertionCount() const { return -1; } \
		void Run() const override; \
	}; \
	Test_##category##_##name g_test_##category##_##name; \
	void Test_##category##_##name::Run() const

#define TEST3(category, name, assertion_count) \
	class Test_##category##_##name : public Test \
	{ \
	public: \
		Test_##category##_##name() { Register(this); } \
		const char* Category() const override { return #category; } \
		const char* Name() const override { return #name; } \
		int GetExpectedAssertionCount() const { return assertion_count; } \
		void Run() const override; \
	}; \
	Test_##category##_##name g_test_##category##_##name; \
	void Test_##category##_##name::Run() const

struct AssertionFailed
{
};
