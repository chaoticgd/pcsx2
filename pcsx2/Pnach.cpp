// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Pnach.h"

#include "Host.h"

#include "common/Assertions.h"
#include "common/StringUtil.h"

#include <limits>

Pnach::PatchPlace Pnach::Patch::Place() const
{
	return m_place;
}

void Pnach::Patch::SetPlace(PatchPlace place)
{
	m_place = place;
}

Pnach::PatchCPU Pnach::Patch::CPU() const
{
	return m_cpu;
}

void Pnach::Patch::SetCPU(PatchCPU cpu)
{
	if (!PatchTypeSupportedForCPU(m_type, cpu))
		m_type = PatchType::WORD;

	m_cpu = cpu;
}

u32 Pnach::Patch::Address() const
{
	return m_address;
}

void Pnach::Patch::SetAddress(u32 address)
{
	m_address = address;
	m_address_has_leading_zeroes = true;
	m_address_is_lowercase = true;
}

Pnach::PatchType Pnach::Patch::Type() const
{
	return m_type;
}

void Pnach::Patch::SetType(PatchType type)
{
	if ((type == PatchType::BYTES) != (m_type == PatchType::BYTES))
	{
		m_data = 0;
		m_bytes.reset(nullptr);
	}

	m_type = type;
}

u64 Pnach::Patch::Data() const
{
	pxAssert(Type() != PatchType::BYTES);
	return m_data;
}

void Pnach::Patch::SetData(u64 data)
{
	pxAssert(Type() != PatchType::BYTES);
	m_data = TruncateDataForPatchType(data, m_type);
	m_data_has_leading_zeroes = true;
	m_data_is_lowercase = true;
}

std::span<const u8> Pnach::Patch::Bytes() const
{
	pxAssert(Type() == PatchType::BYTES);
	return std::span<u8>(m_bytes.get(), m_data);
}

void Pnach::Patch::SetBytes(std::span<const u8> bytes)
{
	pxAssert(Type() == PatchType::BYTES);
	m_data = static_cast<u64>(bytes.size());
	m_bytes.reset(new u8[bytes.size()]);
	std::memcpy(m_bytes.get(), bytes.data(), bytes.size());
}

static bool HexStringIsLowerCase(std::string_view string)
{
	for (char c : string)
		if (c >= 'a' && c <= 'f')
			return true;
		else if (c >= 'A' && c <= 'F')
			return false;

	return true;
}

std::optional<Pnach::Patch> Pnach::Patch::FromString(std::string_view input, Error* error)
{
	Patch patch;

	const std::vector<std::string_view> parameters(StringUtil::SplitString(input, ',', false));
	if (parameters.size() != 5)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected 5: <place>,<cpu>,<address>,<type>,<data>."), parameters.size());
		return std::nullopt;
	}

	const std::optional<PatchPlace> place = PatchPlaceFromString(parameters[0]);
	if (!place.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid place '{}' passed as first patch parameter, expected '0' (once on startup), '1' (continuously), or '2' (both)."), parameters[0]);
		return std::nullopt;
	}

	patch.m_place = *place;

	const std::optional<PatchCPU> cpu = PatchCPUFromString(parameters[1]);
	if (!cpu.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid CPU '{}' passed as second patch parameter, expected 'EE' or 'IOP'."), parameters[1]);
		return std::nullopt;
	}

	patch.m_cpu = *cpu;

	std::string_view address_end;
	const std::optional<u32> address = StringUtil::FromChars<u32>(parameters[2], 16, &address_end);
	if (!address.has_value() || !address_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid address '{}' passed as third patch parameter, expected a hexadecimal number without a prefix."), parameters[2]);
		return std::nullopt;
	}

	patch.m_address = *address;
	patch.m_address_has_leading_zeroes = parameters[2].size() > 1 && parameters[2][0] == '0';
	patch.m_address_is_lowercase = HexStringIsLowerCase(parameters[2]);

	const std::optional<PatchType> type = PatchTypeFromString(parameters[3]);
	if (!type.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid type '{}' passed as fourth patch parameter, expected {}."),
			parameters[3], PatchTypesSupportedForCPU(patch.m_cpu));
		return std::nullopt;
	}

	if (!PatchTypeSupportedForCPU(*type, patch.m_cpu))
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Patch type '{}' passed as fourth patch parameter is incompatible with the specified CPU '{}', expected {}."),
			parameters[3], parameters[1], PatchTypesSupportedForCPU(patch.m_cpu));
		return std::nullopt;
	}

	patch.m_type = *type;

	if (type != PatchType::BYTES)
	{
		std::string_view data_end;
		const std::optional<u64> data = StringUtil::FromChars<u64>(parameters[4], 16, &data_end);
		if (!data.has_value())
		{
			Error::SetStringFmt(error,
				TRANSLATE_FS("Pnach", "Invalid data '{}' passed as fifth patch parameter, expected a hexadecimal number without a prefix."), parameters[4]);
			return std::nullopt;
		}

		patch.m_data = TruncateDataForPatchType(*data, patch.m_type);
		patch.m_data_has_leading_zeroes = parameters[4].size() > 1 && parameters[4][0] == '0';
		patch.m_data_is_lowercase = HexStringIsLowerCase(parameters[4]);
	}
	else
	{
		const std::optional<std::vector<u8>> bytes = StringUtil::DecodeHex(parameters[4]);
		if (!bytes.has_value() || bytes->empty())
		{
			Error::SetStringFmt(error,
				TRANSLATE_FS("Pnach", "Invalid data '{}' passed as fifth patch parameter, expected a hexadecimal string without prefix (e.g. 0123ABCD)."), parameters[4]);
			return std::nullopt;
		}

		patch.m_data = static_cast<u64>(bytes->size());
		patch.m_bytes.reset(new u8[bytes->size()]);
		std::memcpy(patch.m_bytes.get(), bytes->data(), bytes->size());
	}

	return patch;
}

std::string Pnach::Patch::ToString() const
{
	const char* place = PatchPlaceToString(m_place);
	const char* cpu = PatchCPUToString(m_cpu, false);
	const char* type = PatchTypeToString(m_type);

	std::string data;
	if (m_type != PatchType::BYTES)
	{
		size_t data_width = 0;
		if (m_data_has_leading_zeroes)
			data_width = DataSizeFromPatchType(m_type) * 2;

		if (m_data_is_lowercase)
			data = fmt::format("{:0{}x}", m_data, data_width);
		else
			data = fmt::format("{:0{}X}", m_data, data_width);
	}
	else
	{
		data = StringUtil::EncodeHex(m_bytes.get(), m_data);
	}

	size_t address_width = 0;
	if (m_address_has_leading_zeroes)
		address_width = 8;

	if (m_address_is_lowercase)
		return fmt::format("{},{},{:0{}x},{},{}", place, cpu, m_address, address_width, type, data);
	else
		return fmt::format("{},{},{:0{}X},{},{}", place, cpu, m_address, address_width, type, data);
}

// *****************************************************************************

std::span<const Pnach::DynamicPatchEntry> Pnach::DynamicPatch::Pattern() const
{
	return std::span(m_pattern.get(), m_pattern_count);
}

void Pnach::DynamicPatch::SetPattern(std::span<const DynamicPatchEntry> pattern)
{
	size_t count = std::min(pattern.size(), static_cast<size_t>(std::numeric_limits<u32>::max()));

	m_pattern.reset(new DynamicPatchEntry[count]);
	m_pattern_count = static_cast<u32>(count);
	std::memcpy(m_pattern.get(), pattern.data(), count * sizeof(DynamicPatchEntry));

	// Make sure the offsets are aligned to instruction boundaries.
	for (size_t i = 0; i < m_pattern_count; i++)
		m_pattern[i].offset &= ~3;
}

std::span<const Pnach::DynamicPatchEntry> Pnach::DynamicPatch::Replacement() const
{
	return std::span(m_replacement.get(), m_replacement_count);
}

void Pnach::DynamicPatch::SetReplacement(std::span<const DynamicPatchEntry> replacement)
{
	size_t count = std::min(replacement.size(), static_cast<size_t>(std::numeric_limits<u32>::max()));

	m_replacement.reset(new DynamicPatchEntry[count]);
	m_replacement_count = static_cast<u32>(count);
	std::memcpy(m_replacement.get(), replacement.data(), count * sizeof(DynamicPatchEntry));

	// Make sure the offsets are aligned to instruction boundaries.
	for (size_t i = 0; i < m_replacement_count; i++)
		m_replacement[i].offset &= ~3;
}

static bool ParseDynamicPatchEntries(
	std::span<Pnach::DynamicPatchEntry> output,
	const std::vector<std::string_view>& parameters,
	size_t& next_parameter,
	Error* error,
	fmt::runtime_format_string<> offset_error_format_string,
	fmt::runtime_format_string<> value_error_format_string)
{
	for (size_t i = 0; i < output.size(); i++)
	{
		const size_t offset_parameter = next_parameter++;
		const size_t value_parameter = next_parameter++;

		std::string_view offset_end;
		const std::optional<u32> offset = StringUtil::FromChars<u32>(parameters[offset_parameter], 16, &offset_end);
		if (!offset.has_value() || !offset_end.empty() || *offset % 4 != 0)
		{
			Error::SetStringFmt(error, offset_error_format_string, parameters[offset_parameter], i + 1);
			return false;
		}

		output[i].offset = *offset;

		std::string_view value_end;
		const std::optional<u32> value = StringUtil::FromChars<u32>(parameters[value_parameter], 16, &value_end);
		if (!value.has_value() || !value_end.empty())
		{
			Error::SetStringFmt(error, value_error_format_string, parameters[value_parameter], i + 1);
			return false;
		}

		output[i].value = *value;
	}

	return true;
}

std::optional<Pnach::DynamicPatch> Pnach::DynamicPatch::FromString(std::string_view input, Error* error)
{
	Pnach::DynamicPatch patch;

	const std::vector<std::string_view> parameters(StringUtil::SplitString(input, ',', false));
	if (parameters.size() < 3)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected 3 or more: <type>,<pattern count>,<replacement count>,[patterns...],[replacements...]."), parameters.size());
		return std::nullopt;
	}

	std::string_view type_end;
	const std::optional<u32> type = StringUtil::FromChars<u32>(parameters[0], 10, &type_end);
	if (!type.has_value() || !type_end.empty() || *type != 0)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid type '{}' passed as first patch parameter, expected '0' (only currently supported value)."), parameters[0]);
		return std::nullopt;
	}

	std::string_view pattern_count_end;
	const std::optional<u32> pattern_count = StringUtil::FromChars<u32>(parameters[1], 16, &pattern_count_end);
	if (!pattern_count.has_value() || !pattern_count_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid pattern count '{}' passed as second patch parameter, expected a hexadecimal number without a prefix."), parameters[1]);
		return std::nullopt;
	}

	patch.m_pattern.reset(new DynamicPatchEntry[*pattern_count]);
	patch.m_pattern_count = *pattern_count;

	std::string_view replacement_count_end;
	const std::optional<u32> replacement_count = StringUtil::FromChars<u32>(parameters[2], 16, &replacement_count_end);
	if (!replacement_count.has_value() || !replacement_count_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid replacement count '{}' passed as third patch parameter, expected a hexadecimal number without a prefix."), parameters[2]);
		return std::nullopt;
	}

	patch.m_replacement.reset(new DynamicPatchEntry[*replacement_count]);
	patch.m_replacement_count = *replacement_count;

	const size_t expected_parameter_count = 3 + patch.m_pattern_count * 2 + patch.m_replacement_count * 2;
	if (parameters.size() != expected_parameter_count)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected {} (type, pattern count, replacement count, and an offset and size for each pattern and replacement)."),
			parameters.size(), expected_parameter_count);
		return std::nullopt;
	}

	size_t next_parameter = 3;

	const std::span<DynamicPatchEntry> patterns(patch.m_pattern.get(), patch.m_pattern_count);
	if (!ParseDynamicPatchEntries(patterns, parameters, next_parameter, error,
			TRANSLATE_FS("Pnach", "Invalid offset {} passed as parameter of pattern {}, expected a multiple of four as hexadecimal number without a prefix."),
			TRANSLATE_FS("Pnach", "Invalid value {} passed as parameter of pattern {}, expected a hexadecimal number without a prefix.")))
		return std::nullopt;

	const std::span<DynamicPatchEntry> replacements(patch.m_replacement.get(), patch.m_replacement_count);
	if (!ParseDynamicPatchEntries(replacements, parameters, next_parameter, error,
			TRANSLATE_FS("Pnach", "Invalid offset {} passed as parameter of replacement {}, expected a multiple of four as hexadecimal number without a prefix."),
			TRANSLATE_FS("Pnach", "Invalid value {} passed as parameter of replacement {}, expected a hexadecimal number without a prefix.")))
		return std::nullopt;

	return patch;
}

std::string Pnach::DynamicPatch::ToString() const
{
	std::vector<std::string> pieces;
	pieces.reserve(1 + m_pattern_count + m_replacement_count);

	pieces.emplace_back(fmt::format("0,{:x},{:x}", m_pattern_count, m_replacement_count));

	for (u32 i = 0; i < m_pattern_count; i++)
		pieces.emplace_back(fmt::format("{:08x},{:08x}", m_pattern[i].offset, m_pattern[i].value));

	for (u32 i = 0; i < m_replacement_count; i++)
		pieces.emplace_back(fmt::format("{:08x},{:08x}", m_replacement[i].offset, m_replacement[i].value));

	return StringUtil::JoinString(pieces.begin(), pieces.end(), ',');
}

// *****************************************************************************

std::optional<Pnach::GSAspectRatio> Pnach::GSAspectRatio::FromString(std::string_view input, Error* error)
{
	std::string_view string(input);

	std::optional<u32> dividend = StringUtil::FromChars<u32>(string, 10, &string);

	bool has_delimiter = false;
	if (string.size() >= 1 && string[0] == ':')
	{
		has_delimiter = true;
		string = string.substr(1);
	}

	std::optional<u32> divisor = StringUtil::FromChars<u32>(string, 10, &string);

	if (!dividend.has_value() || !has_delimiter || !divisor.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid GS aspect ratio '{}', expected two numbers separated by a colon e.g. '16:9'."), input);
		return std::nullopt;
	}

	GSAspectRatio aspect_ratio;
	aspect_ratio.divisor = *divisor;
	aspect_ratio.dividend = *dividend;
	return aspect_ratio;
}

std::string Pnach::GSAspectRatio::ToString() const
{
	return std::to_string(dividend) + ':' + std::to_string(divisor);
}

// *****************************************************************************

Pnach::CommandType Pnach::Command::Type() const
{
	if (std::holds_alternative<Patch>(m_data))
		return CommandType::PATCH;
	else if (std::holds_alternative<DynamicPatch>(m_data))
		return CommandType::DPATCH;
	else if (std::holds_alternative<GSAspectRatio>(m_data))
		return CommandType::GSASPECTRATIO;
	else if (std::holds_alternative<GSInterlaceMode>(m_data))
		return CommandType::GSINTERLACEMODE;
	else
		return std::get<StringCommand>(m_data).type;
}

Pnach::Patch& Pnach::Command::GetPatch()
{
	Patch* patch = std::get_if<Patch>(&m_data);
	pxAssert(patch);
	return *patch;
}

const Pnach::Patch& Pnach::Command::GetPatch() const
{
	const Patch* patch = std::get_if<Patch>(&m_data);
	pxAssert(patch);
	return *patch;
}

void Pnach::Command::SetPatch(Patch patch)
{
	m_data.emplace<Patch>(std::move(patch));
	ResetFormatting();
}

Pnach::DynamicPatch& Pnach::Command::GetDynamicPatch()
{
	DynamicPatch* dynamic_patch = std::get_if<DynamicPatch>(&m_data);
	pxAssert(dynamic_patch);
	return *dynamic_patch;
}

const Pnach::DynamicPatch& Pnach::Command::GetDynamicPatch() const
{
	const DynamicPatch* dynamic_patch = std::get_if<DynamicPatch>(&m_data);
	pxAssert(dynamic_patch);
	return *dynamic_patch;
}

void Pnach::Command::SetDynamicPatch(DynamicPatch dynamic_patch)
{
	m_data.emplace<DynamicPatch>(std::move(dynamic_patch));
	ResetFormatting();
}

Pnach::GSAspectRatio Pnach::Command::GetGSAspectRatio() const
{
	const GSAspectRatio* aspect_ratio = std::get_if<GSAspectRatio>(&m_data);
	pxAssert(aspect_ratio);
	return *aspect_ratio;
}

void Pnach::Command::SetGSAspectRatio(GSAspectRatio aspect_ratio)
{
	m_data = aspect_ratio;
	ResetFormatting();
}

GSInterlaceMode Pnach::Command::GetGSInterlaceMode() const
{
	const GSInterlaceMode* interlace_mode = std::get_if<GSInterlaceMode>(&m_data);
	pxAssert(interlace_mode);
	return *interlace_mode;
}

void Pnach::Command::SetGSInterlaceMode(GSInterlaceMode interlace_mode)
{
	m_data = interlace_mode;
	ResetFormatting();
}

std::string_view Pnach::Command::GetString() const
{
	const StringCommand* data = std::get_if<StringCommand>(&m_data);
	pxAssert(data);
	return std::string_view(data->string.get(), data->string_size);
}

void Pnach::Command::SetString(CommandType type, std::string_view string, bool reset_formatting)
{
	pxAssert(type != CommandType::PATCH);
	pxAssert(type != CommandType::DPATCH);
	pxAssert(type != CommandType::GSASPECTRATIO);
	pxAssert(type != CommandType::GSINTERLACEMODE);

	StringCommand& data = m_data.emplace<StringCommand>();
	data.type = type;
	data.string.reset(new char[string.size()]);
	std::memcpy(data.string.get(), string.data(), string.size());
	data.string_size = string.size();

	if (reset_formatting)
		ResetFormatting();
}

void Pnach::Command::SetSpacer()
{
	m_data = StringCommand{CommandType::SPACER};
}

std::string_view Pnach::Command::EndOfLineComment() const
{
	return std::string_view(m_end_of_line_comment.get(), m_end_of_line_comment_size);
}

void Pnach::Command::SetEndOfLineComment(std::string_view comment, bool reset_formatting)
{
	const size_t size = std::min(comment.size(), static_cast<size_t>(std::numeric_limits<u16>::max()));

	m_end_of_line_comment.reset(new char[size]);
	std::memcpy(m_end_of_line_comment.get(), comment.data(), size);
	m_end_of_line_comment_size = static_cast<u16>(size);

	if (reset_formatting)
		ResetFormatting();
}

void Pnach::Command::RemoveEndOfLineComment()
{
	m_end_of_line_comment.reset(nullptr);
	ResetFormatting();
}

void Pnach::Command::ResetFormatting()
{
	m_spaces_at_start_of_line = 0;
	m_spaces_before_assignment_operator = 0;
	m_spaces_after_assignment_operator = 0;
	m_spaces_before_end_of_line_comment_delimiter = 1;
	m_spaces_after_end_of_line_comment_delimiter = 1;
}

Pnach::Command Pnach::Command::FromString(std::string_view input, Error* error)
{
	Command command;

	const std::string_view::size_type comment_delimiter_pos = input.find("//");

	const std::string_view raw_assignment = input.substr(0, comment_delimiter_pos);
	const std::string_view assignment = StringUtil::StripWhitespace(raw_assignment);

	if (!assignment.empty() && !command.ParseAssignment(assignment, error))
		command.SetString(CommandType::INVALID, assignment, false);

	const size_t spaces_at_start_of_line = std::min(
		static_cast<size_t>(assignment.begin() - raw_assignment.begin()),
		static_cast<size_t>(std::numeric_limits<u8>::max()));
	command.m_spaces_at_start_of_line = static_cast<u8>(spaces_at_start_of_line);

	if (comment_delimiter_pos != std::string_view::npos)
	{
		std::string_view raw_comment = input.substr(comment_delimiter_pos + 2);
		std::string_view comment = StringUtil::StripWhitespace(raw_comment);

		command.SetEndOfLineComment(comment, false);

		const size_t spaces_before_end_of_line_comment_delimiter = std::min(
			static_cast<size_t>(raw_assignment.end() - assignment.end()),
			static_cast<size_t>(std::numeric_limits<u8>::max()));
		command.m_spaces_before_end_of_line_comment_delimiter = static_cast<u8>(spaces_before_end_of_line_comment_delimiter);

		const size_t spaces_after_end_of_line_comment_delimiter = std::min(
			static_cast<size_t>(comment.begin() - raw_comment.begin()),
			static_cast<size_t>(std::numeric_limits<u8>::max()));
		command.m_spaces_after_end_of_line_comment_delimiter = static_cast<u8>(spaces_after_end_of_line_comment_delimiter);
	}

	return command;
}

bool Pnach::Command::ParseAssignment(std::string_view assignment, Error* error)
{
	std::string_view::size_type assignment_operator_pos = assignment.find('=');
	if (assignment_operator_pos == std::string_view::npos)
		return false;

	const std::string_view raw_key = assignment.substr(0, assignment_operator_pos);
	const std::string_view raw_value = assignment.substr(assignment_operator_pos + 1);

	const std::string_view key = StringUtil::StripWhitespace(raw_key);
	const std::string_view value = StringUtil::StripWhitespace(raw_value);

	if (key == "patch")
	{
		std::optional<Patch> patch = Patch::FromString(value, error);
		if (!patch.has_value())
			return false;

		m_data.emplace<Patch>(std::move(*patch));
	}
	else if (key == "dpatch")
	{
		std::optional<DynamicPatch> dynamic_patch = DynamicPatch::FromString(value, error);
		if (!dynamic_patch.has_value())
			return false;

		m_data.emplace<DynamicPatch>(std::move(*dynamic_patch));
	}
	else if (key == "gsaspectratio")
	{
		std::optional<GSAspectRatio> aspect_ratio = GSAspectRatio::FromString(value, error);
		if (!aspect_ratio.has_value())
			return false;

		m_data.emplace<GSAspectRatio>(std::move(*aspect_ratio));
	}
	else if (key == "gsinterlacemode")
	{
		std::string_view interlace_mode_end;
		const std::optional<u8> interlace_mode = StringUtil::FromChars<u8>(value, 10, &interlace_mode_end);
		if (!interlace_mode.has_value() ||
			!interlace_mode_end.empty() ||
			interlace_mode.value() >= static_cast<u8>(GSInterlaceMode::Count))
		{
			Error::SetStringFmt(error,
				TRANSLATE_FS("Pnach", "Invalid GS interlace mode '{}', expected a decimal number between 0 and {}."),
				value, static_cast<u8>(GSInterlaceMode::Count) - 1);
			return false;
		}

		m_data.emplace<GSInterlaceMode>(static_cast<GSInterlaceMode>(*interlace_mode));
	}
	else if (key == "author")
	{
		SetString(CommandType::AUTHOR, value, false);
	}
	else if (key == "comment")
	{
		SetString(CommandType::COMMENT, value, false);
	}
	else if (key == "description")
	{
		SetString(CommandType::DESCRIPTION, value, false);
	}
	else if (key == "gametitle")
	{
		SetString(CommandType::GAMETITLE, value, false);
	}
	else
	{
		return false;
	}

	const size_t spaces_before_assignment_operator = std::min(
		static_cast<size_t>(raw_key.end() - key.end()),
		static_cast<size_t>(std::numeric_limits<u8>::max()));
	m_spaces_before_assignment_operator = static_cast<u8>(spaces_before_assignment_operator);

	const size_t spaces_after_assignment_operator = std::min(
		static_cast<size_t>(value.begin() - raw_value.begin()),
		static_cast<size_t>(std::numeric_limits<u8>::max()));
	m_spaces_after_assignment_operator = static_cast<u8>(spaces_after_assignment_operator);

	return true;
}

std::string Pnach::Command::ToString() const
{
	std::string result;

	for (size_t i = 0; i < m_spaces_at_start_of_line; i++)
		result += ' ';

	if (const Patch* patch = std::get_if<Patch>(&m_data))
	{
		result += "patch";
		AppendAssignmentOperator(result);
		result += patch->ToString();
	}
	else if (const DynamicPatch* dynamic_patch = std::get_if<DynamicPatch>(&m_data))
	{
		result += "dpatch";
		AppendAssignmentOperator(result);
		result += dynamic_patch->ToString();
	}
	else if (const GSAspectRatio* aspect_ratio = std::get_if<GSAspectRatio>(&m_data))
	{
		result += "gsaspectratio";
		AppendAssignmentOperator(result);
		result += std::to_string(aspect_ratio->dividend);
		result += ':';
		result += std::to_string(aspect_ratio->divisor);
	}
	else if (const GSInterlaceMode* interlace_mode = std::get_if<GSInterlaceMode>(&m_data))
	{
		result += "gsinterlacemode";
		AppendAssignmentOperator(result);
		result += std::to_string(static_cast<u8>(*interlace_mode));
	}
	else
	{
		const StringCommand& command = std::get<StringCommand>(m_data);

		switch (command.type)
		{
			case CommandType::AUTHOR:
			{
				result += "author";
				AppendAssignmentOperator(result);
				break;
			}
			case CommandType::COMMENT:
			{
				result += "comment";
				AppendAssignmentOperator(result);
				break;
			}
			case CommandType::DESCRIPTION:
			{
				result += "description";
				AppendAssignmentOperator(result);
				break;
			}
			case CommandType::GAMETITLE:
			{
				result += "gametitle";
				AppendAssignmentOperator(result);
				break;
			}
			default:
			{
			}
		}

		result += std::string_view(command.string.get(), command.string_size);
	}

	if (m_end_of_line_comment.get())
	{
		if (!result.empty())
			for (size_t i = 0; i < m_spaces_before_end_of_line_comment_delimiter; i++)
				result += ' ';

		result += "//";

		if (!EndOfLineComment().empty())
			for (size_t i = 0; i < m_spaces_after_end_of_line_comment_delimiter; i++)
				result += ' ';

		result += EndOfLineComment();
	}

	return result;
}

void Pnach::Command::AppendAssignmentOperator(std::string& string) const
{
	for (size_t i = 0; i < m_spaces_before_assignment_operator; i++)
		string += ' ';

	string += '=';

	for (size_t i = 0; i < m_spaces_after_assignment_operator; i++)
		string += ' ';
}

// *****************************************************************************

bool Pnach::PatchTypeSupportedForCPU(PatchType type, PatchCPU cpu)
{
	bool supported = false;

	switch (cpu)
	{
		case PatchCPU::EE:
			supported = true;
			break;
		case PatchCPU::IOP:
			supported = type == PatchType::BYTE ||
			            type == PatchType::SHORT ||
			            type == PatchType::WORD ||
			            type == PatchType::BYTES;
			break;
	}

	return supported;
}

std::string Pnach::PatchTypesSupportedForCPU(PatchCPU cpu)
{
	if (cpu == PatchCPU::EE)
		return TRANSLATE("Pnach", "'byte', 'short', 'word', 'double', 'beshort', 'beword', 'bedouble', 'bytes' or 'extended'");
	else
		return TRANSLATE("Pnach", "'byte', 'short', 'word' or 'bytes");
}

size_t Pnach::DataSizeFromPatchType(PatchType type)
{
	switch (type)
	{
		case PatchType::BYTE:
			return 1;
		case PatchType::SHORT:
			return 2;
		case PatchType::WORD:
			return 4;
		case PatchType::DOUBLE:
			return 8;
		case PatchType::BE_SHORT:
			return 2;
		case PatchType::BE_WORD:
			return 4;
		case PatchType::BE_DOUBLE:
			return 8;
		case PatchType::BYTES:
			return 0;
		case PatchType::EXTENDED:
			return 4;
	}

	return 0;
}

u64 Pnach::TruncateDataForPatchType(u64 data, PatchType type)
{
	u64 shift_amount = DataSizeFromPatchType(type) * 8;
	if (shift_amount < 64)
		return data & ((static_cast<u64>(1) << shift_amount) - 1);
	else
		return data;
}

// *****************************************************************************

static constexpr std::array<const char*, Pnach::PATCH_PLACE_COUNT> s_place_names = {
	"0",
	"1",
	"2",
};

static constexpr std::array<const char*, Pnach::PATCH_PLACE_COUNT> s_long_place_names = {
	TRANSLATE_NOOP("Pnach", "On Load"),
	TRANSLATE_NOOP("Pnach", "Continuously"),
	TRANSLATE_NOOP("Pnach", "On Load & Continuously"),
};

static constexpr std::array<const char*, Pnach::PATCH_CPU_COUNT> s_cpu_names = {
	TRANSLATE_NOOP("Pnach", "EE"),
	TRANSLATE_NOOP("Pnach", "IOP"),
};

static constexpr std::array<const char*, Pnach::PATCH_CPU_COUNT> s_long_cpu_names = {
	TRANSLATE_NOOP("Pnach", "Emotion Engine"),
	TRANSLATE_NOOP("Pnach", "Input/Output Processor"),
};

static constexpr std::array<const char*, Pnach::PATCH_TYPE_COUNT> s_type_names = {
	"byte",
	"short",
	"word",
	"double",
	"beshort",
	"beword",
	"bedouble",
	"bytes",
	"extended",
};

std::optional<Pnach::PatchPlace> Pnach::PatchPlaceFromString(std::string_view string)
{
	for (size_t i = 0; i < s_place_names.size(); i++)
		if (string == s_place_names[i])
			return static_cast<Pnach::PatchPlace>(i);

	return std::nullopt;
}

const char* Pnach::PatchPlaceToString(PatchPlace place)
{
	return s_place_names.at(static_cast<size_t>(place));
}

const char* Pnach::PatchPlaceToLongString(PatchPlace cpu, bool translate)
{
	const char* name = s_long_place_names.at(static_cast<size_t>(cpu));

	if (translate)
		name = TRANSLATE("Pnach", name);

	return name;
}


std::optional<Pnach::PatchCPU> Pnach::PatchCPUFromString(std::string_view string)
{
	for (size_t i = 0; i < s_cpu_names.size(); i++)
		if (string == s_cpu_names[i])
			return static_cast<Pnach::PatchCPU>(i);

	return std::nullopt;
}

const char* Pnach::PatchCPUToString(PatchCPU cpu, bool translate)
{
	const char* name = s_cpu_names.at(static_cast<size_t>(cpu));

	if (translate)
		name = TRANSLATE("Pnach", name);

	return name;
}

const char* Pnach::PatchCPUToLongString(PatchCPU cpu, bool translate)
{
	const char* name = s_long_cpu_names.at(static_cast<size_t>(cpu));

	if (translate)
		name = TRANSLATE("Pnach", name);

	return name;
}

std::optional<Pnach::PatchType> Pnach::PatchTypeFromString(std::string_view string)
{
	for (size_t i = 0; i < s_type_names.size(); i++)
		if (string == s_type_names[i])
			return static_cast<Pnach::PatchType>(i);

	return std::nullopt;
}

const char* Pnach::PatchTypeToString(PatchType type)
{
	return s_type_names.at(static_cast<size_t>(type));
}
