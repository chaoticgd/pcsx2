// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "pcsx2/Pnach.h"
#include <gtest/gtest.h>

TEST(Pnach, PatchFromString)
{
	std::optional<Pnach::Patch> patch = Pnach::Patch::FromString("1,EE,00100000,short,1234");
	ASSERT_TRUE(patch.has_value());
	ASSERT_EQ(patch->Place(), Pnach::PatchPlace::CONTINUOUSLY);
	ASSERT_EQ(patch->CPU(), Pnach::PatchCPU::EE);
	ASSERT_EQ(patch->Address(), 0x100000u);
	ASSERT_EQ(patch->Type(), Pnach::PatchType::SHORT);
	ASSERT_EQ(patch->Data(), 0x1234u);

	std::optional<Pnach::Patch> bytes_patch = Pnach::Patch::FromString("1,EE,00100000,bytes,1234");
	ASSERT_TRUE(bytes_patch.has_value());
	ASSERT_EQ(bytes_patch->Place(), Pnach::PatchPlace::CONTINUOUSLY);
	ASSERT_EQ(bytes_patch->CPU(), Pnach::PatchCPU::EE);
	ASSERT_EQ(bytes_patch->Address(), 0x100000u);
	ASSERT_EQ(bytes_patch->Type(), Pnach::PatchType::BYTES);
	std::array<u8, 2> bytes = {0x12u, 0x34u};
	ASSERT_EQ(bytes_patch->Bytes().size(), bytes.size());
	ASSERT_EQ(std::memcmp(bytes_patch->Bytes().data(), bytes.data(), bytes.size()), 0);
}

TEST(Pnach, PatchFromStringInvalid)
{
	std::optional<Pnach::Patch> no_commas = Pnach::Patch::FromString("hello");
	ASSERT_FALSE(no_commas.has_value());

	std::optional<Pnach::Patch> too_many_commas = Pnach::Patch::FromString(",,,,,");
	ASSERT_FALSE(too_many_commas.has_value());

	std::optional<Pnach::Patch> invalid_place = Pnach::Patch::FromString("3,EE,0,byte,0");
	ASSERT_FALSE(invalid_place.has_value());

	std::optional<Pnach::Patch> invalid_cpu = Pnach::Patch::FromString("1,washingmachine,0,byte,0");
	ASSERT_FALSE(invalid_cpu.has_value());

	std::optional<Pnach::Patch> invalid_address = Pnach::Patch::FromString("1,EE,123 Fake Street,byte,0");
	ASSERT_FALSE(invalid_address.has_value());

	std::optional<Pnach::Patch> junk_after_address = Pnach::Patch::FromString("1,EE,100000?,byte,0");
	ASSERT_FALSE(junk_after_address.has_value());

	std::optional<Pnach::Patch> invalid_type = Pnach::Patch::FromString("1,EE,0,qubit,0");
	ASSERT_FALSE(invalid_type.has_value());

	std::optional<Pnach::Patch> incompatible_type = Pnach::Patch::FromString("1,IOP,0,extended,0");
	ASSERT_FALSE(incompatible_type.has_value());

	std::optional<Pnach::Patch> invalid_data = Pnach::Patch::FromString("1,EE,0,byte,hello");
	ASSERT_FALSE(invalid_data.has_value());

	std::optional<Pnach::Patch> invalid_bytes_data = Pnach::Patch::FromString("1,EE,0,bytes,hello");
	ASSERT_FALSE(invalid_bytes_data.has_value());

	std::optional<Pnach::Patch> empty_bytes_data = Pnach::Patch::FromString("1,EE,0,bytes,");
	ASSERT_FALSE(invalid_bytes_data.has_value());
}

TEST(Pnach, PatchToString)
{
	Pnach::Patch patch;
	patch.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	patch.SetCPU(Pnach::PatchCPU::EE);
	patch.SetAddress(0x100000u);
	patch.SetType(Pnach::PatchType::SHORT);
	patch.SetData(0x1234u);
	EXPECT_EQ(patch.ToString(), "1,EE,00100000,short,1234");

	Pnach::Patch bytes_patch;
	bytes_patch.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	bytes_patch.SetCPU(Pnach::PatchCPU::EE);
	bytes_patch.SetAddress(0x100000u);
	bytes_patch.SetType(Pnach::PatchType::BYTES);
	std::array<u8, 2> bytes = {0x12u, 0x34u};
	bytes_patch.SetBytes(bytes);
	EXPECT_EQ(bytes_patch.ToString(), "1,EE,00100000,bytes,1234");
}

#define PATCH_ROUND_TRIP_TEST(parameters) \
	{ \
		std::optional<Pnach::Patch> patch; \
		patch = Pnach::Patch::FromString(parameters); \
		ASSERT_TRUE(patch.has_value()); \
		EXPECT_EQ(patch->ToString(), parameters); \
	}

TEST(Pnach, PatchPreserveFormatting)
{
	// Leading zeroes in the address parameter.
	PATCH_ROUND_TRIP_TEST("1,EE,0,word,0");
	PATCH_ROUND_TRIP_TEST("1,EE,100000,word,0");
	PATCH_ROUND_TRIP_TEST("1,EE,00100000,word,0");

	// Leading zeroes in the data parameter.
	PATCH_ROUND_TRIP_TEST("1,EE,0,byte,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,byte,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,byte,00");
	PATCH_ROUND_TRIP_TEST("1,EE,0,byte,01");
	PATCH_ROUND_TRIP_TEST("1,EE,0,byte,11");

	PATCH_ROUND_TRIP_TEST("1,EE,0,short,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,short,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,short,0000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,short,0001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,short,1111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,word,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,word,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,word,00000000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,word,00000001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,word,11111111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,double,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,double,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,double,0000000000000000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,double,0000000000000001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,double,1111111111111111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,beshort,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beshort,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beshort,0000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beshort,0001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beshort,1111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,beword,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beword,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beword,00000000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beword,00000001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,beword,11111111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,bedouble,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,bedouble,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,bedouble,0000000000000000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,bedouble,0000000000000001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,bedouble,1111111111111111");

	PATCH_ROUND_TRIP_TEST("1,EE,0,extended,0");
	PATCH_ROUND_TRIP_TEST("1,EE,0,extended,1");
	PATCH_ROUND_TRIP_TEST("1,EE,0,extended,00000000");
	PATCH_ROUND_TRIP_TEST("1,EE,0,extended,00000001");
	PATCH_ROUND_TRIP_TEST("1,EE,0,extended,11111111");

	// Case of address parameter.
	PATCH_ROUND_TRIP_TEST("1,EE,1234abcd,word,00000000");
	PATCH_ROUND_TRIP_TEST("1,EE,1234ABCD,word,00000000");

	// Case of data parameter.
	PATCH_ROUND_TRIP_TEST("1,EE,00100000,word,1234abcd");
	PATCH_ROUND_TRIP_TEST("1,EE,00100000,word,1234ABCD");
}

TEST(Pnach, PatchOverrideFormatting)
{
	// Leading zeroes.
	std::optional<Pnach::Patch> padding_patch = Pnach::Patch::FromString("1,EE,0,word,0");
	ASSERT_TRUE(padding_patch.has_value());

	padding_patch->SetAddress(0x100000u);
	padding_patch->SetData(0x1234u);

	EXPECT_EQ(padding_patch->ToString(), "1,EE,00100000,word,00001234");

	// Case.
	std::optional<Pnach::Patch> case_patch = Pnach::Patch::FromString("1,EE,0012ABCD,word,1234ABCD");
	ASSERT_TRUE(case_patch.has_value());

	case_patch->SetAddress(case_patch->Address());
	case_patch->SetData(case_patch->Data());

	EXPECT_EQ(case_patch->ToString(), "1,EE,0012abcd,word,1234abcd");
}

#define PATCH_TRUNCATE_TEST(parameters, expected) \
	{ \
		std::optional<Pnach::Patch> patch; \
		patch = Pnach::Patch::FromString(parameters); \
		ASSERT_TRUE(patch.has_value()); \
		EXPECT_EQ(patch->ToString(), expected); \
	}

TEST(Pnach, PatchTruncateDataForType)
{
	PATCH_TRUNCATE_TEST("1,EE,0,byte,1234", "1,EE,0,byte,34");
	PATCH_TRUNCATE_TEST("1,EE,0,short,1234abcd", "1,EE,0,short,abcd");
	PATCH_TRUNCATE_TEST("1,EE,0,word,12345678abcd", "1,EE,0,word,5678abcd");
}

// *****************************************************************************

TEST(Pnach, DynamicPatchFromString)
{
	std::optional<Pnach::DynamicPatch> simple = Pnach::DynamicPatch::FromString(
		"0,1,1,00000000,03e00008,00000004,25080001");
	ASSERT_TRUE(simple.has_value());
	ASSERT_TRUE(simple->Pattern().size() == 1);
	EXPECT_TRUE(simple->Pattern()[0].offset == 0);
	EXPECT_TRUE(simple->Pattern()[0].value == 0x03e00008);
	ASSERT_TRUE(simple->Replacement().size() == 1);
	EXPECT_TRUE(simple->Replacement()[0].offset == 4);
	EXPECT_TRUE(simple->Replacement()[0].value == 0x25080001);

	std::optional<Pnach::DynamicPatch> different_counts = Pnach::DynamicPatch::FromString(
		"0,2,1,00000000,03e00008,00000004,00000000,00000004,25080001");
	ASSERT_TRUE(different_counts.has_value());
	ASSERT_TRUE(different_counts->Pattern().size() == 2);
	EXPECT_TRUE(different_counts->Pattern()[0].offset == 0);
	EXPECT_TRUE(different_counts->Pattern()[0].value == 0x03e00008);
	EXPECT_TRUE(different_counts->Pattern()[1].offset == 4);
	EXPECT_TRUE(different_counts->Pattern()[1].value == 0);
	ASSERT_TRUE(different_counts->Replacement().size() == 1);
	EXPECT_TRUE(different_counts->Replacement()[0].offset == 4);
	EXPECT_TRUE(different_counts->Replacement()[0].value == 0x25080001);
}

TEST(Pnach, DynamicPatchFromStringInvalid)
{
	std::optional<Pnach::DynamicPatch> no_commas = Pnach::DynamicPatch::FromString("hello");
	ASSERT_FALSE(no_commas.has_value());

	std::optional<Pnach::DynamicPatch> too_few_commas = Pnach::DynamicPatch::FromString("0,1,1");
	ASSERT_FALSE(too_few_commas.has_value());

	std::optional<Pnach::DynamicPatch> too_many_commas = Pnach::DynamicPatch::FromString("0,0,0,0,0,0,0");
	ASSERT_FALSE(too_few_commas.has_value());

	std::optional<Pnach::DynamicPatch> invalid_type = Pnach::DynamicPatch::FromString("123,0,0");
	ASSERT_FALSE(invalid_type.has_value());

	std::optional<Pnach::DynamicPatch> invalid_pattern_count = Pnach::DynamicPatch::FromString("0,0hello,0");
	ASSERT_FALSE(invalid_pattern_count.has_value());

	std::optional<Pnach::DynamicPatch> invalid_replacement_count = Pnach::DynamicPatch::FromString("0,0,0hello");
	ASSERT_FALSE(invalid_replacement_count.has_value());

	std::optional<Pnach::DynamicPatch> too_few_parameters = Pnach::DynamicPatch::FromString("0,1,0,0");
	ASSERT_FALSE(too_few_parameters.has_value());

	std::optional<Pnach::DynamicPatch> too_many_parameters = Pnach::DynamicPatch::FromString("0,1,0,0,0,0,0");
	ASSERT_FALSE(too_many_parameters.has_value());

	std::optional<Pnach::DynamicPatch> invalid_pattern_offset = Pnach::DynamicPatch::FromString("0,1,0,0hello,0");
	ASSERT_FALSE(invalid_pattern_offset.has_value());

	std::optional<Pnach::DynamicPatch> unaligned_pattern_offset = Pnach::DynamicPatch::FromString("0,1,0,3,0");
	ASSERT_FALSE(unaligned_pattern_offset.has_value());

	std::optional<Pnach::DynamicPatch> invalid_pattern_value = Pnach::DynamicPatch::FromString("0,1,0,0,0hello");
	ASSERT_FALSE(invalid_pattern_value.has_value());

	std::optional<Pnach::DynamicPatch> invalid_replacement_offset = Pnach::DynamicPatch::FromString("0,0,1,0hello,0");
	ASSERT_FALSE(invalid_replacement_offset.has_value());

	std::optional<Pnach::DynamicPatch> unaligned_replacement_offset = Pnach::DynamicPatch::FromString("0,0,1,3,0");
	ASSERT_FALSE(unaligned_replacement_offset.has_value());

	std::optional<Pnach::DynamicPatch> invalid_replacement_value = Pnach::DynamicPatch::FromString("0,0,1,0,0hello");
	ASSERT_FALSE(invalid_replacement_value.has_value());
}

TEST(Pnach, DynamicPatchToString)
{
	Pnach::DynamicPatch simple;
	std::array<Pnach::DynamicPatchEntry, 1> simple_pattern = {{
		{
			.offset = 0,
			.value = 0x03e00008,
		},
	}};
	simple.SetPattern(simple_pattern);
	std::array<Pnach::DynamicPatchEntry, 1> simple_replacement = {{
		{
			.offset = 4,
			.value = 0x25080001,
		},
	}};
	simple.SetReplacement(simple_replacement);
	EXPECT_EQ(simple.ToString(), "0,1,1,00000000,03e00008,00000004,25080001");

	Pnach::DynamicPatch different_counts;
	std::array<Pnach::DynamicPatchEntry, 2> different_counts_pattern = {{
		{
			.offset = 0,
			.value = 0x03e00008,
		},
		{
			.offset = 4,
			.value = 0x00000000,
		},
	}};
	different_counts.SetPattern(different_counts_pattern);
	std::array<Pnach::DynamicPatchEntry, 1> different_counts_replacement = {{
		{
			.offset = 4,
			.value = 0x25080001,
		},
	}};
	different_counts.SetReplacement(different_counts_replacement);
	EXPECT_EQ(different_counts.ToString(), "0,2,1,00000000,03e00008,00000004,00000000,00000004,25080001");
}
