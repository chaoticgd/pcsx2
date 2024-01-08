// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QString>

#include "common/Pcsx2Defs.h"
#include "DebugTools/ccc/ast.h"

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
	ccc::NodeHandle type;
	DataInspectorLocation location;
	DataInspectorNode* parent = nullptr;
	std::vector<std::unique_ptr<DataInspectorNode>> children;
	bool children_fetched = false;
};
