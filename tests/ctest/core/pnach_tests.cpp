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
	ASSERT_EQ(patch.ToString(), "1,EE,00100000,short,1234");

	Pnach::Patch bytes_patch;
	bytes_patch.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	bytes_patch.SetCPU(Pnach::PatchCPU::EE);
	bytes_patch.SetAddress(0x100000u);
	bytes_patch.SetType(Pnach::PatchType::BYTES);
	std::array<u8, 2> bytes = {0x12u, 0x34u};
	bytes_patch.SetBytes(bytes);
	ASSERT_EQ(bytes_patch.ToString(), "1,EE,00100000,bytes,1234");
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
	EXPECT_EQ(Pnach::Patch::FromString(parameters)->ToString(), expected)


TEST(Pnach, PatchTruncateDataForType)
{
	PATCH_TRUNCATE_TEST("1,EE,0,byte,1234", "1,EE,0,byte,34");
	PATCH_TRUNCATE_TEST("1,EE,0,short,1234abcd", "1,EE,0,short,abcd");
	PATCH_TRUNCATE_TEST("1,EE,0,word,12345678abcd", "1,EE,0,word,5678abcd");
}
