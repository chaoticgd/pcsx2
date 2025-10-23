// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Pnach.h"

#include "Host.h"

#include "common/Assertions.h"
#include "common/StringUtil.h"

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

std::optional<Pnach::Patch> Pnach::Patch::FromString(std::string_view parameters, Error* error)
{
	Patch patch;

	const std::vector<std::string_view> pieces(StringUtil::SplitString(parameters, ',', false));
	if (pieces.size() != 5)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected 5: <place>,<cpu>,<address>,<type>,<data>."), pieces.size());
		return std::nullopt;
	}

	const std::optional<PatchPlace> place = PatchPlaceFromString(pieces[0]);
	if (!place.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid place '{}' passed as first patch parameter, expected '0' (once on startup), '1' (continuously), or '2' (both)."), pieces[0]);
		return std::nullopt;
	}

	patch.m_place = *place;

	const std::optional<PatchCPU> cpu = PatchCPUFromString(pieces[1]);
	if (!cpu.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid CPU '{}' passed as second patch parameter, expected 'EE' or 'IOP'."), pieces[1]);
		return std::nullopt;
	}

	patch.m_cpu = *cpu;

	std::string_view address_end;
	const std::optional<u32> address = StringUtil::FromChars<u32>(pieces[2], 16, &address_end);
	if (!address.has_value() || !address_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid address '{}' passed as third patch parameter, expected a hexadecimal number without a prefix."), pieces[2]);
		return std::nullopt;
	}

	patch.m_address = *address;
	patch.m_address_has_leading_zeroes = pieces[2].size() > 1 && pieces[2][0] == '0';
	patch.m_address_is_lowercase = HexStringIsLowerCase(pieces[2]);

	const std::optional<PatchType>
		type = PatchTypeFromString(pieces[3]);
	if (!type.has_value())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid type '{}' passed as fourth patch parameter, expected {}."),
			pieces[3], PatchTypesSupportedForCPU(patch.m_cpu));
		return std::nullopt;
	}
	if (!PatchTypeSupportedForCPU(*type, patch.m_cpu))
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Patch type '{}' passed as fourth patch parameter is incompatible with the specified CPU '{}', expected {}."),
			pieces[3], pieces[1], PatchTypesSupportedForCPU(patch.m_cpu));
		return std::nullopt;
	}

	patch.m_type = *type;

	if (type != PatchType::BYTES)
	{
		std::string_view data_end;
		const std::optional<u64> data = StringUtil::FromChars<u64>(pieces[4], 16, &data_end);
		if (!data.has_value())
		{
			Error::SetStringFmt(error,
				TRANSLATE_FS("Pnach", "Invalid data '{}' passed as fifth patch parameter, expected a hexadecimal number without a prefix."), pieces[4]);
			return std::nullopt;
		}

		patch.m_data = TruncateDataForPatchType(*data, patch.m_type);
		patch.m_data_has_leading_zeroes = pieces[4].size() > 1 && pieces[4][0] == '0';
		patch.m_data_is_lowercase = HexStringIsLowerCase(pieces[4]);
	}
	else
	{
		const std::optional<std::vector<u8>> bytes = StringUtil::DecodeHex(pieces[4]);
		if (!bytes.has_value() || bytes->empty())
		{
			Error::SetStringFmt(error,
				TRANSLATE_FS("Pnach", "Invalid data '{}' passed as fifth patch parameter, expected a hexadecimal string without prefix (e.g. 0123ABCD)."), pieces[4]);
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

bool Pnach::DynamicPatch::SetPattern(std::span<const DynamicPatchEntry> pattern)
{
	if (pattern.size() > UINT32_MAX)
		return false;

	m_pattern.reset(new DynamicPatchEntry[pattern.size()]);
	m_pattern_count = static_cast<u32>(pattern.size());
	std::memcpy(m_pattern.get(), pattern.data(), pattern.size() * sizeof(DynamicPatchEntry));

	// Make sure the offsets are aligned to instruction boundaries.
	for (size_t i = 0; i < m_pattern_count; i++)
		m_pattern[i].offset &= ~3;

	return true;
}

std::span<const Pnach::DynamicPatchEntry> Pnach::DynamicPatch::Replacement() const
{
	return std::span(m_replacement.get(), m_replacement_count);
}

bool Pnach::DynamicPatch::SetReplacement(std::span<const DynamicPatchEntry> replacement)
{
	if (replacement.size() > UINT32_MAX)
		return false;

	m_replacement.reset(new DynamicPatchEntry[replacement.size()]);
	m_replacement_count = static_cast<u32>(replacement.size());
	std::memcpy(m_replacement.get(), replacement.data(), replacement.size() * sizeof(DynamicPatchEntry));

	// Make sure the offsets are aligned to instruction boundaries.
	for (size_t i = 0; i < m_pattern_count; i++)
		m_pattern[i].offset &= ~3;

	return true;
}

static bool ParseDynamicPatchEntries(
	std::span<Pnach::DynamicPatchEntry> output,
	const std::vector<std::string_view>& pieces,
	size_t& next_parameter,
	Error* error,
	fmt::runtime_format_string<> offset_error_format_string,
	fmt::runtime_format_string<> value_error_format_string)
{
	for (size_t i = 0; i < output.size(); i++)
	{
		size_t offset_parameter = next_parameter++;
		std::string_view offset_end;
		const std::optional<u32> offset = StringUtil::FromChars<u32>(pieces[offset_parameter], 16, &offset_end);
		if (!offset.has_value() || !offset_end.empty() || *offset % 4 != 0)
		{
			Error::SetStringFmt(error, offset_error_format_string, pieces[offset_parameter], i + 1);
			return false;
		}

		output[i].offset = *offset;

		size_t value_parameter = next_parameter++;
		std::string_view value_end;
		const std::optional<u32> value = StringUtil::FromChars<u32>(pieces[value_parameter], 16, &value_end);
		if (!value.has_value() || !value_end.empty())
		{
			Error::SetStringFmt(error, value_error_format_string, pieces[value_parameter], i + 1);
			return false;
		}

		output[i].value = *value;
	}

	return true;
}

std::optional<Pnach::DynamicPatch> Pnach::DynamicPatch::FromString(std::string_view parameters, Error* error)
{
	Pnach::DynamicPatch patch;

	const std::vector<std::string_view> pieces(StringUtil::SplitString(parameters, ',', false));
	if (pieces.size() < 3)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected 3 or more: <type>,<pattern count>,<replacement count>,[patterns...],[replacements...]."), pieces.size());
		return std::nullopt;
	}

	std::string_view type_end;
	const std::optional<u32> type = StringUtil::FromChars<u32>(pieces[0], 10, &type_end);
	if (!type.has_value() || !type_end.empty() || *type != 0)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid type '{}' passed as first patch parameter, expected '0' (only currently supported value)."), pieces[0]);
		return std::nullopt;
	}

	std::string_view pattern_count_end;
	const std::optional<u32> pattern_count = StringUtil::FromChars<u32>(pieces[1], 16, &pattern_count_end);
	if (!pattern_count.has_value() || !pattern_count_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid pattern count '{}' passed as second patch parameter, expected a hexadecimal number without a prefix."), pieces[1]);
		return std::nullopt;
	}

	patch.m_pattern.reset(new DynamicPatchEntry[*pattern_count]);
	patch.m_pattern_count = *pattern_count;

	std::string_view replacement_count_end;
	const std::optional<u32> replacement_count = StringUtil::FromChars<u32>(pieces[2], 16, &replacement_count_end);
	if (!replacement_count.has_value() || !replacement_count_end.empty())
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Invalid replacement count '{}' passed as third patch parameter, expected a hexadecimal number without a prefix."), pieces[2]);
		return std::nullopt;
	}

	patch.m_replacement.reset(new DynamicPatchEntry[*replacement_count]);
	patch.m_replacement_count = *replacement_count;

	size_t expected_parameter_count = 3 + patch.m_pattern_count * 2 + patch.m_replacement_count * 2;
	if (pieces.size() != expected_parameter_count)
	{
		Error::SetStringFmt(error,
			TRANSLATE_FS("Pnach", "Found {} comma-separated patch parameters, expected {} (type, pattern count, replacement count, and an offset and size for each pattern and replacement)."),
			pieces.size(), expected_parameter_count);
		return std::nullopt;
	}

	size_t next_parameter = 3;

	std::span<DynamicPatchEntry> patterns(patch.m_pattern.get(), patch.m_pattern_count);
	if (!ParseDynamicPatchEntries(patterns, pieces, next_parameter, error,
			TRANSLATE_FS("Pnach", "Invalid offset {} passed as parameter of pattern {}, expected a multiple of four as hexadecimal number without a prefix."),
			TRANSLATE_FS("Pnach", "Invalid value {} passed as parameter of pattern {}, expected a hexadecimal number without a prefix.")))
		return std::nullopt;

	std::span<DynamicPatchEntry> replacements(patch.m_replacement.get(), patch.m_replacement_count);
	if (!ParseDynamicPatchEntries(replacements, pieces, next_parameter, error,
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
