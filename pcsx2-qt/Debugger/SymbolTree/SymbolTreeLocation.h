// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QString>

#include "common/Pcsx2Types.h"
#include "DebugTools/DebugInterface.h"

class DebugInterface;

// A memory location, either a register or an address.
struct SymbolTreeLocation
{
	enum Type
	{
		REGISTER,
		MEMORY,
		NONE // Put NONE last so nodes of this type sort to the bottom.
	};
	
	Type type = NONE;
	u32 address = 0;

	SymbolTreeLocation();
	SymbolTreeLocation(Type type_arg, u32 address_arg);

	QString name(DebugInterface& cpu) const;

	SymbolTreeLocation addOffset(u32 offset) const;

	u8 read8(DebugInterface& cpu);
	u16 read16(DebugInterface& cpu);
	u32 read32(DebugInterface& cpu);
	u64 read64(DebugInterface& cpu);
	u128 read128(DebugInterface& cpu);

	void write8(u8 value, DebugInterface& cpu);
	void write16(u16 value, DebugInterface& cpu);
	void write32(u32 value, DebugInterface& cpu);
	void write64(u64 value, DebugInterface& cpu);
	void write128(u128 value, DebugInterface& cpu);

	friend auto operator<=>(const SymbolTreeLocation& lhs, const SymbolTreeLocation& rhs) = default;
};
