/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>

#include "common/Pcsx2Defs.h"
#include "DebugTools/ccc/analysis.h"

class DebugInterface;

struct DataInspectorLocation
{
	enum
	{
		NONE,
		EE_REGISTER,
		IOP_REGISTER,
		EE_MEMORY,
		IOP_MEMORY,
	} type = NONE;
	u32 address = 0;
	
	QString name() const;
	
	DataInspectorLocation addOffset(u32 offset) const;
	DataInspectorLocation createAddress(u32 address) const;
	
	DebugInterface& cpu() const;
	
	u8 read8();
	u16 read16();
	u32 read32();
	u64 read64();
	u128 read128();
	
	void write8(u8 value);
	void write16(u16 value);
	void write32(u32 value);
	void write64(u64 value);
	void write128(u128 value);
};

struct DataInspectorNode
{
	QString name;
	const ccc::ast::Node* type = nullptr;
	DataInspectorLocation location;
	DataInspectorNode* parent = nullptr;
	std::vector<std::unique_ptr<DataInspectorNode>> children;
	bool childrenFetched = false;
};
