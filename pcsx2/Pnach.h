// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Config.h"

#include "common/Error.h"
#include "common/Pcsx2Defs.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// This is the pnach patch parser used by the graphical patch editor in the
// debugger. It is geared towards being able to preserving formatting and
// comments and is not used by the runtime patching system.

namespace Pnach
{
	/// The point in time where the patch is applied.
	enum class PatchPlace : u8
	{
		ONCE_ON_LOAD = 0,
		CONTINUOUSLY = 1,
		ONCE_ON_LOAD_AND_CONTINUOUSLY = 2
	};
	inline constexpr u32 PATCH_PLACE_COUNT = 3;

	/// The cpu parameter.
	enum class PatchCPU : u8
	{
		EE = 0,
		IOP = 1
	};
	inline constexpr u32 PATCH_CPU_COUNT = 2;

	/// The type parameter.
	enum class PatchType : u8
	{
		BYTE = 0,
		SHORT = 1,
		WORD = 2,
		DOUBLE = 3,
		BE_SHORT = 4,
		BE_WORD = 5,
		BE_DOUBLE = 6,
		BYTES = 7,
		EXTENDED = 8
	};
	inline constexpr u32 PATCH_TYPE_COUNT = 9;

	/// A single patch command. These are for patching code or data at fixed
	/// addresses and are used for the majority of patches.
	class Patch
	{
	public:
		/// Access the place parameter.
		PatchPlace Place() const;
		void SetPlace(PatchPlace place);

		/// Access the CPU parameter.
		PatchCPU CPU() const;
		void SetCPU(PatchCPU cpu);

		/// Access the raw address parameter. Note that for patches of type
		/// EXTENDED this may not be an actual address.
		u32 Address() const;
		void SetAddress(u32 address);

		/// Access the raw type parameter. Note that for patches of type
		/// EXTENDED there will be a secondary opcode stored in the address
		/// parameter.
		PatchType Type() const;
		void SetType(PatchType type);

		/// Access the data for patches not of type BYTES.
		u64 Data() const;
		void SetData(u64 data);

		/// Access the data for patches of type BYTES.
		std::span<const u8> Bytes() const;
		void SetBytes(std::span<const u8> bytes);

		/// Parse the parameters of a patch command, which should be a
		/// comma-separated list of values in the following format:
		///   <place>,<cpu>,<address>,<type>,<data>
		static std::optional<Patch> FromString(std::string_view input, Error* error = nullptr);

		/// Convert the patch back to a string containing a comma-separated list
		/// of values (see the FromString function).
		std::string ToString() const;

	private:
		/// These two members change their meaning depending on if m_type is
		/// equal to BYTES or not. If it is, the data is stored in m_bytes and
		/// is of length m_data, otherwise the data is stored in m_data.
		u64 m_data = 0;
		std::unique_ptr<u8[]> m_bytes;

		u32 m_address = 0;

		PatchPlace m_place = PatchPlace::CONTINUOUSLY;
		PatchCPU m_cpu = PatchCPU::EE;
		PatchType m_type = PatchType::WORD;

		/// Save how the patch was formatted in the text file so we can avoid
		/// modifying it unnecessarily when writing it out. For simplicity only
		/// some common formatting choices have been implemented.
		bool m_address_has_leading_zeroes : 1 = true;
		bool m_address_is_lowercase : 1 = true;
		bool m_data_has_leading_zeroes : 1 = true;
		bool m_data_is_lowercase : 1 = true;
	};

	/// A single instruction to be matched or replaced.
	struct DynamicPatchEntry
	{
		/// The offset from the instruction currently being recompiled.
		u32 offset = 0;

		/// The memory value to match or write, depending on whether this is a
		/// pattern or replacement entry.
		u32 value = 0;
	};

	/// A single dynamic patch command. These are used when code (but not data)
	/// moves around in memory and so a patch for that code cannot operate on a
	/// fixed address.
	class DynamicPatch
	{
	public:
		std::span<const DynamicPatchEntry> Pattern() const;
		void SetPattern(std::span<const DynamicPatchEntry> pattern);

		std::span<const DynamicPatchEntry> Replacement() const;
		void SetReplacement(std::span<const DynamicPatchEntry> replacement);

		/// Parse the parameters of a patch command, which should be a
		/// variable-length comma-separated list in the following format:
		///   <type>,<pattern count>,<replacement count>,[patterns...],[replacements...]
		/// where each pattern and replacement is in the following format:
		///   <offset>,<value>
		static std::optional<DynamicPatch> FromString(std::string_view parameters, Error* error = nullptr);

		/// Convert the patch back to a string containing a comma-separated list
		/// of values (see the FromString function).
		std::string ToString() const;

	private:
		// Make the DynamicPatch class smaller, since it seems like it would be
		// a shame to bloat every single patch for such a rarely used feature.
		std::unique_ptr<DynamicPatchEntry[]> m_pattern;
		std::unique_ptr<DynamicPatchEntry[]> m_replacement;
		u32 m_pattern_count = 0;
		u32 m_replacement_count = 0;
	};

	struct GSAspectRatio
	{
		u32 dividend = 0;
		u32 divisor = 0;

		static std::optional<GSAspectRatio> FromString(std::string_view input, Error* error = nullptr);
		std::string ToString() const;
	};

	enum class PatchLineType
	{
		PATCH, // Patch.
		DPATCH, // Dynamic patch.

		GSASPECTRATIO,
		GSINTERLACEMODE,

		AUTHOR,
		COMMENT,
		DESCRIPTION,
		GAMETITLE,

		SPACER, // The line is empty (except for if an end of line comment exists).
		INVALID, // The line could not be parsed.
	};

	/// A single line in a .pnach file.
	class PatchLine
	{
	public:
		/// Retrieve the type of this line.
		PatchLineType Type() const;

		/// Access the contained patch. Only valid for PATCH lines.
		Patch& GetPatch();
		const Patch& GetPatch() const;

		/// Change the type to PATCH.
		void SetPatch(Patch patch);

		/// Access the contained dynamic patch. Only valid for DPATCH lines.
		DynamicPatch& GetDynamicPatch();
		const DynamicPatch& GetDynamicPatch() const;

		/// Change the type to DPATCH.
		void SetDynamicPatch(DynamicPatch dynamic_patch);

		/// Retrieve the contained GS aspect ratio.
		GSAspectRatio GetGSAspectRatio() const;

		/// Change the type to GSAPSECTRATIO.
		void SetGSAspectRatio(GSAspectRatio aspect_ratio);

		/// Retrieve the contained GS interlace mode.
		GSInterlaceMode GetGSInterlaceMode() const;

		//// Change the type to GSINTERLACEMODE.
		void SetGSInterlaceMode(GSInterlaceMode interlace_mode);

		/// Retrieve the contained string. Not valid for PATCH, DPATCH,
		/// GSASPECTRATIO or GSINTERLACEMODE lines.
		std::string_view GetString() const;

		/// Change the type to the one specified (except for PATCH, DPATCH,
		/// GSASPECTRATIO and GSINTERLACEMODE) and store the pased string.
		void SetString(PatchLineType type, std::string_view string, bool reset_formatting = true);

		/// Change the type to SPACER.
		void SetSpacer();

		/// Retrieve the end of line comment.
		std::string_view EndOfLineComment() const;

		/// Update the end of line comment. This does not change the line type.
		void SetEndOfLineComment(std::string_view comment, bool reset_formatting = true);

		/// Remove the end of line comment.
		void RemoveEndOfLineComment();

		/// Reset the numbers of spaces between different parts of the line to
		// the default values.
		void ResetFormatting();

		/// Parse a patch line from a .pnach file.
		static PatchLine FromString(std::string_view input, Error* error = nullptr);

		/// Convert the line to a string, including a comment if one exists.
		std::string ToString() const;

	private:
		/// Parse a patch line that has had its comment removed.
		bool ParseAssignment(std::string_view assignment, Error* error = nullptr);
		void AppendAssignmentOperator(std::string& string) const;

		struct StringLine
		{
			PatchLineType type;
			std::unique_ptr<char[]> string;
			size_t string_size = 0;
		};

		using PatchLineData = std::variant<Patch, DynamicPatch, GSAspectRatio, GSInterlaceMode, StringLine>;
		PatchLineData m_data = StringLine{PatchLineType::SPACER};

		// Try to save some space here since in the worst case, where someone
		// tries to fill memory using patches, comments will be rare.
		std::unique_ptr<char[]> m_end_of_line_comment;
		u16 m_end_of_line_comment_size = 0;
		u8 m_spaces_at_start_of_line = 0;
		u8 m_spaces_before_assignment_operator = 0;
		u8 m_spaces_after_assignment_operator = 0;
		u8 m_spaces_before_end_of_line_comment_delimiter = 1;
		u8 m_spaces_after_end_of_line_comment_delimiter = 1;
	};

	bool PatchTypeSupportedForCPU(PatchType type, PatchCPU cpu);
	std::string PatchTypesSupportedForCPU(PatchCPU cpu);
	size_t DataSizeFromPatchType(PatchType type);
	u64 TruncateDataForPatchType(u64 data, PatchType type);

	std::optional<PatchPlace> PatchPlaceFromString(std::string_view string);
	const char* PatchPlaceToString(PatchPlace place);
	const char* PatchPlaceToLongString(PatchPlace cpu, bool translate);
	std::optional<PatchCPU> PatchCPUFromString(std::string_view string);
	const char* PatchCPUToString(PatchCPU cpu, bool translate);
	const char* PatchCPUToLongString(PatchCPU cpu, bool translate);
	std::optional<PatchType> PatchTypeFromString(std::string_view string);
	const char* PatchTypeToString(PatchType type);
} // namespace Pnach
