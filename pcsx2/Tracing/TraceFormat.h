// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#ifdef _MSC_VER
#define TM_STRUCT(name, ...) \
	__pragma(pack(push, 1)) struct alignas(4) name \
	{ \
		__VA_ARGS__ \
	} __pragma(pack(pop));
#else
#define TM_STRUCT(name, ...) \
	struct __attribute__((__packed__)) alignas(4) name \
	{ \
		__VA_ARGS__ \
	};
#endif

namespace Tracing
{
	inline u32 TRACE_FILE_FORMAT_VERSION = 1;

	TM_STRUCT(MapHeader,
		u64 offset;
		u32 count; // The number of entries in the map table.
	)
	
	TM_STRUCT(FileHeader,
		u32 magic;
		u32 version;
		u32 pcsx2_version;
		u32 flags;
		u64 data_offset;
		u64 string_area_offset;
		u64 string_area_size;
		MapHeader event_map;
		MapHeader channel_map;
		MapHeader memory_map;
	)
	
	TM_STRUCT(MemoryMapEntry,
		u32 string;
		u32 offset;
		u32 size;
	)

	namespace FileFlags
	{
		enum
		{
			FINISHED = 1 << 0
		};
	};

	enum class PacketType : u16
	{
		INVALID,
		SAVE_STATE,
		BEGIN_EVENT,
		END_EVENT,
		WRITE
	};

	TM_STRUCT(PacketHeader,
		PacketType type;
		u32 size;
		char data[];
	)
	
	enum class EventType : u16
	{
		INSTRUCTION_EXECUTED
	};

	enum class Channel : u8
	{
		R5900,
		R5900_INSTRUCTIONS_EXECUTED
	};

	TM_STRUCT(EventPacket,
		u16 event;
		u8 channel;
		u8 thread;
		u32 timestamp;
		u32 args[];
	)
	
	enum class MemorySize : u8
	{
		BITS_8,
		BITS_16,
		BITS_32,
		BITS_64,
		BITS_128
	};
	
	TM_STRUCT(WritePacket,
		u32 address;
		u128 value;
	)
}; // namespace TimeMachine

#undef TRACE_STRUCT
