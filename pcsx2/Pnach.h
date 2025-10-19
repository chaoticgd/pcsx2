// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
	// The point in time where the patch is applied.
	enum class PatchPlace : u8
	{
		ONCE_ON_LOAD = 0,
		CONTINUOUSLY = 1,
		ONCE_ON_LOAD_AND_CONTINUOUSLY = 2
	};
	inline constexpr u32 PATCH_PLACE_COUNT = 3;

	// The cpu parameter.
	enum class PatchCPU : u8
	{
		EE = 0,
		IOP = 1
	};
	inline constexpr u32 PATCH_CPU_COUNT = 2;

	// The type parameter.
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

	// The parameters for a single patch command.
	class Patch
	{
	public:
		// Accesses the place parameter.
		PatchPlace Place() const;
		void SetPlace(PatchPlace place);

		// Accesses the CPU parameter.
		PatchCPU CPU() const;
		void SetCPU(PatchCPU cpu);

		// Access the raw address parameter. Note that for patches of type
		// EXTENDED this may not be an actual address.
		u32 Address() const;
		void SetAddress(u32 address);

		// Accesses the raw type parameter. Note that for patches of type
		// EXTENDED there will be a secondary opcode stored in the address
		// parameter.
		PatchType Type() const;
		void SetType(PatchType type);

		// Accesses the data for patches not of type BYTES.
		u64 Data() const;
		void SetData(u64 data);

		// Accesses the data for patches of type BYTES.
		std::span<const u8> Bytes() const;
		void SetBytes(std::span<const u8> bytes);

		// Parse the parameter of the patch command, which should be a
		// comma-separated list of values in the following format:
		//   <place>,<cpu>,<address>,<type>,<data>
		static std::optional<Patch> FromString(std::string_view parameter, Error* error = nullptr);

		// Convert the patch back to a string containing a comma-separated list
		// of values (see the from_string function).
		std::string ToString() const;

	private:
		// These two members change their meaning depending on if m_type is
		// equal to BYTES or not. If it is, the data is stored in m_bytes and is
		// of length m_data, otherwise the data is stored directly in m_data.
		u64 m_data = 0;
		std::unique_ptr<u8[]> m_bytes;

		u32 m_address = 0;

		PatchPlace m_place = PatchPlace::CONTINUOUSLY;
		PatchCPU m_cpu = PatchCPU::EE;
		PatchType m_type = PatchType::WORD;

		// Save how the patch was formatted in the text file so we can avoid
		// modifying it unnecessarily when writing it out. For simplicity only
		// some common formatting choices have been implemented.
		bool m_address_has_leading_zeroes : 1 = true;
		bool m_address_is_lowercase : 1 = true;
		bool m_data_has_leading_zeroes : 1 = true;
		bool m_data_is_lowercase : 1 = true;
	};

	bool PatchTypeSupportedForCPU(PatchType type, PatchCPU cpu);
	size_t DataSizeFromPatchType(PatchType type);
	u64 TruncateDataForPatchType(u64 data, PatchType type);

	std::optional<PatchPlace> PatchPlaceFromString(std::string_view string);
	const char* PatchPlaceToString(PatchPlace place);
	const char* PatchPlaceToLongString(PatchPlace cpu, bool translate);
	std::optional<PatchCPU> PatchCPUFromString(std::string_view string);
	const char* PatchCPUToString(PatchCPU cpu, bool translate);
	const char* PatchCPUToLongString(PatchCPU cpu, bool translate);
	std::optional<PatchType> PatchTypeFromString(std::string_view string);
	const char* PatchTypeToString(PatchType type, bool translate);
} // namespace Pnach
