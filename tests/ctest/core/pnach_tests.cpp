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

	std::optional<Pnach::Patch> load_patch = Pnach::Patch::FromString("0,EE,00100000,short,1234");
	ASSERT_TRUE(load_patch.has_value());
	ASSERT_EQ(load_patch->Place(), Pnach::PatchPlace::ONCE_ON_LOAD);
	ASSERT_EQ(load_patch->CPU(), Pnach::PatchCPU::EE);
	ASSERT_EQ(load_patch->Address(), 0x100000u);
	ASSERT_EQ(load_patch->Type(), Pnach::PatchType::SHORT);
	ASSERT_EQ(load_patch->Data(), 0x1234u);

	std::optional<Pnach::Patch> iop_patch = Pnach::Patch::FromString("1,IOP,00100000,short,1234");
	ASSERT_TRUE(iop_patch.has_value());
	ASSERT_EQ(iop_patch->Place(), Pnach::PatchPlace::CONTINUOUSLY);
	ASSERT_EQ(iop_patch->CPU(), Pnach::PatchCPU::IOP);
	ASSERT_EQ(iop_patch->Address(), 0x100000u);
	ASSERT_EQ(iop_patch->Type(), Pnach::PatchType::SHORT);
	ASSERT_EQ(iop_patch->Data(), 0x1234u);

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

	Pnach::Patch load_patch;
	load_patch.SetPlace(Pnach::PatchPlace::ONCE_ON_LOAD);
	load_patch.SetCPU(Pnach::PatchCPU::EE);
	load_patch.SetAddress(0x100000u);
	load_patch.SetType(Pnach::PatchType::SHORT);
	load_patch.SetData(0x1234u);
	EXPECT_EQ(load_patch.ToString(), "0,EE,00100000,short,1234");

	Pnach::Patch iop_patch;
	iop_patch.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	iop_patch.SetCPU(Pnach::PatchCPU::IOP);
	iop_patch.SetAddress(0x100000u);
	iop_patch.SetType(Pnach::PatchType::SHORT);
	iop_patch.SetData(0x1234u);
	EXPECT_EQ(iop_patch.ToString(), "1,IOP,00100000,short,1234");

	Pnach::Patch bytes_patch;
	bytes_patch.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	bytes_patch.SetCPU(Pnach::PatchCPU::EE);
	bytes_patch.SetAddress(0x100000u);
	bytes_patch.SetType(Pnach::PatchType::BYTES);
	std::array<u8, 2> bytes = {0x12u, 0x34u};
	bytes_patch.SetBytes(bytes);
	EXPECT_EQ(bytes_patch.ToString(), "1,EE,00100000,bytes,1234");
}

#define PATCH_TRUNCATE_TEST(input, expected) \
	{ \
		std::optional<Pnach::Patch> patch; \
		patch = Pnach::Patch::FromString(input); \
		ASSERT_TRUE(patch.has_value()); \
		EXPECT_EQ(patch->ToString(), expected); \
	}

TEST(Pnach, PatchTruncateDataForType)
{
	PATCH_TRUNCATE_TEST("1,EE,00000000,byte,1234", "1,EE,00000000,byte,34");
	PATCH_TRUNCATE_TEST("1,EE,00000000,short,1234abcd", "1,EE,00000000,short,abcd");
	PATCH_TRUNCATE_TEST("1,EE,00000000,word,12345678abcd", "1,EE,00000000,word,5678abcd");
}

// *****************************************************************************

TEST(Pnach, DynamicPatchFromString)
{
	std::optional<Pnach::DynamicPatch> simple = Pnach::DynamicPatch::FromString(
		"0,1,1,00000000,03e00008,00000004,25080001");
	ASSERT_TRUE(simple.has_value());
	ASSERT_EQ(simple->Pattern().size(), 1u);
	EXPECT_EQ(simple->Pattern()[0].offset, 0u);
	EXPECT_EQ(simple->Pattern()[0].value, 0x03e00008u);
	ASSERT_EQ(simple->Replacement().size(), 1u);
	EXPECT_EQ(simple->Replacement()[0].offset, 4u);
	EXPECT_EQ(simple->Replacement()[0].value, 0x25080001u);

	std::optional<Pnach::DynamicPatch> more_patterns = Pnach::DynamicPatch::FromString(
		"0,2,1,00000000,03e00008,00000004,00000000,00000004,25080001");
	ASSERT_TRUE(more_patterns.has_value());
	ASSERT_EQ(more_patterns->Pattern().size(), 2u);
	EXPECT_EQ(more_patterns->Pattern()[0].offset, 0u);
	EXPECT_EQ(more_patterns->Pattern()[0].value, 0x03e00008u);
	EXPECT_EQ(more_patterns->Pattern()[1].offset, 4u);
	EXPECT_EQ(more_patterns->Pattern()[1].value, 0u);
	ASSERT_EQ(more_patterns->Replacement().size(), 1u);
	EXPECT_EQ(more_patterns->Replacement()[0].offset, 4u);
	EXPECT_EQ(more_patterns->Replacement()[0].value, 0x25080001u);

	std::optional<Pnach::DynamicPatch> more_replacements = Pnach::DynamicPatch::FromString(
		"0,1,2,00000000,03e00008,00000004,25080001,00000008,00000000");
	ASSERT_TRUE(more_replacements.has_value());
	ASSERT_EQ(more_replacements->Pattern().size(), 1u);
	EXPECT_EQ(more_replacements->Pattern()[0].offset, 0u);
	EXPECT_EQ(more_replacements->Pattern()[0].value, 0x03e00008u);
	ASSERT_EQ(more_replacements->Replacement().size(), 2u);
	EXPECT_EQ(more_replacements->Replacement()[0].offset, 4u);
	EXPECT_EQ(more_replacements->Replacement()[0].value, 0x25080001u);
	EXPECT_EQ(more_replacements->Replacement()[1].offset, 8u);
	EXPECT_EQ(more_replacements->Replacement()[1].value, 0u);
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

	Pnach::DynamicPatch more_patterns;
	std::array<Pnach::DynamicPatchEntry, 2> more_patterns_pattern = {{
		{
			.offset = 0,
			.value = 0x03e00008,
		},
		{
			.offset = 4,
			.value = 0x00000000,
		},
	}};
	more_patterns.SetPattern(more_patterns_pattern);
	std::array<Pnach::DynamicPatchEntry, 1> more_patterns_replacement = {{
		{
			.offset = 4,
			.value = 0x25080001,
		},
	}};
	more_patterns.SetReplacement(more_patterns_replacement);
	EXPECT_EQ(more_patterns.ToString(), "0,2,1,00000000,03e00008,00000004,00000000,00000004,25080001");


	Pnach::DynamicPatch more_replacements;
	std::array<Pnach::DynamicPatchEntry, 1> more_replacements_pattern = {{
		{
			.offset = 0,
			.value = 0x03e00008,
		},
	}};
	more_replacements.SetPattern(more_replacements_pattern);
	std::array<Pnach::DynamicPatchEntry, 2> more_replacements_replacement = {{
		{
			.offset = 4,
			.value = 0x25080001,
		},
		{
			.offset = 8,
			.value = 0x00000000,
		},
	}};
	more_replacements.SetReplacement(more_replacements_replacement);
	EXPECT_EQ(more_replacements.ToString(), "0,1,2,00000000,03e00008,00000004,25080001,00000008,00000000");
}

TEST(Pnach, DynamicPatchAlignment)
{
	Pnach::DynamicPatch dynamic_patch;

	std::array<Pnach::DynamicPatchEntry, 1> pattern = {{
		{
			.offset = 1,
			.value = 0,
		},
	}};
	dynamic_patch.SetPattern(pattern);

	ASSERT_EQ(dynamic_patch.Pattern().size(), 1u);
	ASSERT_EQ(dynamic_patch.Pattern()[0].offset, 0u);

	std::array<Pnach::DynamicPatchEntry, 1> replacement = {{
		{
			.offset = 1,
			.value = 0,
		},
	}};
	dynamic_patch.SetReplacement(replacement);

	ASSERT_EQ(dynamic_patch.Replacement().size(), 1u);
	ASSERT_EQ(dynamic_patch.Replacement()[0].offset, 0u);
}

// *****************************************************************************

TEST(Pnach, GSAspectRatioFromString)
{
	std::optional<Pnach::GSAspectRatio> widescreen = Pnach::GSAspectRatio::FromString("16:9");
	ASSERT_TRUE(widescreen.has_value());
	EXPECT_EQ(widescreen->dividend, 16u);
	EXPECT_EQ(widescreen->divisor, 9u);
}

TEST(Pnach, GSAspectRatioFromStringInvalid)
{
	std::optional<Pnach::GSAspectRatio> empty = Pnach::GSAspectRatio::FromString("");
	ASSERT_FALSE(empty.has_value());

	std::optional<Pnach::GSAspectRatio> single_number = Pnach::GSAspectRatio::FromString("169");
	ASSERT_FALSE(single_number.has_value());

	std::optional<Pnach::GSAspectRatio> wrong_delimiter = Pnach::GSAspectRatio::FromString("16/9");
	ASSERT_FALSE(wrong_delimiter.has_value());
}

// *****************************************************************************

TEST(Pnach, CommandFromString)
{
	Pnach::Command patch = Pnach::Command::FromString("patch=1,EE,00100000,short,1234");
	ASSERT_EQ(patch.Type(), Pnach::CommandType::PATCH);
	ASSERT_EQ(patch.GetPatch().Place(), Pnach::PatchPlace::CONTINUOUSLY);
	ASSERT_EQ(patch.GetPatch().CPU(), Pnach::PatchCPU::EE);
	ASSERT_EQ(patch.GetPatch().Address(), 0x00100000u);
	ASSERT_EQ(patch.GetPatch().Type(), Pnach::PatchType::SHORT);
	ASSERT_EQ(patch.GetPatch().Data(), 0x1234u);

	Pnach::Command dynamic_patch = Pnach::Command::FromString("dpatch=0,0,0");
	ASSERT_EQ(dynamic_patch.Type(), Pnach::CommandType::DPATCH);
	EXPECT_EQ(dynamic_patch.GetDynamicPatch().Pattern().size(), 0u);
	EXPECT_EQ(dynamic_patch.GetDynamicPatch().Replacement().size(), 0u);

	Pnach::Command aspect_ratio = Pnach::Command::FromString("gsaspectratio=16:9");
	ASSERT_EQ(aspect_ratio.Type(), Pnach::CommandType::GSASPECTRATIO);
	EXPECT_EQ(aspect_ratio.GetGSAspectRatio().dividend, 16u);
	EXPECT_EQ(aspect_ratio.GetGSAspectRatio().divisor, 9u);

	Pnach::Command interlace_mode = Pnach::Command::FromString("gsinterlacemode=0");
	ASSERT_EQ(interlace_mode.Type(), Pnach::CommandType::GSINTERLACEMODE);
	EXPECT_EQ(interlace_mode.GetGSInterlaceMode(), GSInterlaceMode::Automatic);

	Pnach::Command author = Pnach::Command::FromString("author=David");
	ASSERT_EQ(author.Type(), Pnach::CommandType::AUTHOR);
	EXPECT_EQ(author.GetString(), "David");

	Pnach::Command comment = Pnach::Command::FromString("comment=Cause bug");
	ASSERT_EQ(comment.Type(), Pnach::CommandType::COMMENT);
	EXPECT_EQ(comment.GetString(), "Cause bug");

	Pnach::Command description = Pnach::Command::FromString("description=Fix bug");
	ASSERT_EQ(description.Type(), Pnach::CommandType::DESCRIPTION);
	EXPECT_EQ(description.GetString(), "Fix bug");

	Pnach::Command gametitle = Pnach::Command::FromString("gametitle=Spacewar!");
	ASSERT_EQ(gametitle.Type(), Pnach::CommandType::GAMETITLE);
	EXPECT_EQ(gametitle.GetString(), "Spacewar!");

	Pnach::Command spacer = Pnach::Command::FromString("");
	ASSERT_EQ(spacer.Type(), Pnach::CommandType::SPACER);

	Pnach::Command invalid = Pnach::Command::FromString("?=?");
	ASSERT_EQ(invalid.Type(), Pnach::CommandType::INVALID);

	Pnach::Command end_of_line_comment = Pnach::Command::FromString("// Hello world");
	ASSERT_EQ(end_of_line_comment.Type(), Pnach::CommandType::SPACER);
	EXPECT_EQ(end_of_line_comment.EndOfLineComment(), "Hello world");

	Pnach::Command empty_comment = Pnach::Command::FromString("//");
	ASSERT_EQ(empty_comment.Type(), Pnach::CommandType::SPACER);
	EXPECT_EQ(empty_comment.EndOfLineComment(), "");

	Pnach::Command patch_with_comment = Pnach::Command::FromString("dpatch=0,0,0 // do thing");
	ASSERT_EQ(patch_with_comment.Type(), Pnach::CommandType::DPATCH);
	EXPECT_EQ(patch_with_comment.EndOfLineComment(), "do thing");

	Pnach::Command patch_with_compact_comment = Pnach::Command::FromString("dpatch=0,0,0//patch the game");
	ASSERT_EQ(patch_with_compact_comment.Type(), Pnach::CommandType::DPATCH);
	EXPECT_EQ(patch_with_compact_comment.EndOfLineComment(), "patch the game");

	Pnach::Command whitespace_at_start_of = Pnach::Command::FromString(" patch=1,EE,00100000,short,1234");
	ASSERT_EQ(whitespace_at_start_of.Type(), Pnach::CommandType::PATCH);

	Pnach::Command whitespace_before_assignment = Pnach::Command::FromString("patch =1,EE,00100000,short,1234");
	ASSERT_EQ(whitespace_before_assignment.Type(), Pnach::CommandType::PATCH);

	Pnach::Command whitespace_after_assignment = Pnach::Command::FromString("patch= 1,EE,00100000,short,1234");
	ASSERT_EQ(whitespace_after_assignment.Type(), Pnach::CommandType::PATCH);
}

TEST(Pnach, CommandToString)
{
	Pnach::Patch patch_command;
	patch_command.SetPlace(Pnach::PatchPlace::CONTINUOUSLY);
	patch_command.SetCPU(Pnach::PatchCPU::EE);
	patch_command.SetAddress(0x00100000);
	patch_command.SetType(Pnach::PatchType::SHORT);
	patch_command.SetData(0x1234);
	Pnach::Command patch;
	patch.SetPatch(std::move(patch_command));
	EXPECT_EQ(patch.ToString(), "patch=1,EE,00100000,short,1234");

	Pnach::Command dynamic_patch;
	dynamic_patch.SetDynamicPatch(Pnach::DynamicPatch());
	EXPECT_EQ(dynamic_patch.ToString(), "dpatch=0,0,0");

	Pnach::Command aspect_ratio;
	aspect_ratio.SetGSAspectRatio(Pnach::GSAspectRatio{.dividend = 16, .divisor = 9});
	EXPECT_EQ(aspect_ratio.ToString(), "gsaspectratio=16:9");

	Pnach::Command interlace_mode;
	interlace_mode.SetGSInterlaceMode(GSInterlaceMode::Automatic);
	EXPECT_EQ(interlace_mode.ToString(), "gsinterlacemode=0");

	Pnach::Command author;
	author.SetString(Pnach::CommandType::AUTHOR, "David");
	EXPECT_EQ(author.ToString(), "author=David");

	Pnach::Command comment;
	comment.SetString(Pnach::CommandType::COMMENT, "Cause bug");
	EXPECT_EQ(comment.ToString(), "comment=Cause bug");

	Pnach::Command description;
	description.SetString(Pnach::CommandType::DESCRIPTION, "Fix bug");
	EXPECT_EQ(description.ToString(), "description=Fix bug");

	Pnach::Command gametitle;
	gametitle.SetString(Pnach::CommandType::GAMETITLE, "Spacewar!");
	EXPECT_EQ(gametitle.ToString(), "gametitle=Spacewar!");

	Pnach::Command spacer;
	EXPECT_EQ(spacer.ToString(), "");

	Pnach::Command invalid;
	invalid.SetString(Pnach::CommandType::INVALID, "?=?");
	EXPECT_EQ(invalid.ToString(), "?=?");

	Pnach::Command end_of_line_comment;
	end_of_line_comment.SetEndOfLineComment("Hello world");
	EXPECT_EQ(end_of_line_comment.ToString(), "// Hello world");

	Pnach::Command empty_comment;
	empty_comment.SetEndOfLineComment("");
	EXPECT_EQ(empty_comment.ToString(), "//");

	Pnach::Command patch_with_comment;
	patch_with_comment.SetDynamicPatch(Pnach::DynamicPatch());
	patch_with_comment.SetEndOfLineComment("do thing");
	EXPECT_EQ(patch_with_comment.ToString(), "dpatch=0,0,0 // do thing");
}
