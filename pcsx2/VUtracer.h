/*
	vutrace - Hacky VU tracer/debugger.
	Copyright (C) 2020-2023 chaoticgd

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include "MemoryTypes.h"
#include "R5900.h"

enum VUTracePacketType {
	VUTRACE_NULLPACKET = 0,
	VUTRACE_PUSHSNAPSHOT = 'P',
	VUTRACE_SETREGISTERS = 'R',
	VUTRACE_SETMEMORY = 'M',
	VUTRACE_SETINSTRUCTIONS = 'I',
	VUTRACE_LOADOP = 'L',
	VUTRACE_STOREOP = 'S',
	VUTRACE_PATCHREGISTER = 'r',
	VUTRACE_PATCHMEMORY = 'm'
};

enum VUTraceStatus {
	VUTRACESTATUS_DISABLED,
	VUTRACESTATUS_WAITING, // We're waiting for vsync.
	VUTRACESTATUS_TRACING
};

struct VURegs;
class VUTracer {
public:
	VUTracer();
	
	void onTraceMenuItemClicked();
	void onVsync();
	void onVif1DmaSendChain(u32 tadr);
	void onVifDmaTag(u32 madr, u64 dma_tag);
	void onVu1ExecMicro(u32 pc);
	void onInstructionExecuted(VURegs* regs);
	void onMemoryRead(u32 addr, u32 size);
	void onMemoryWrite(u32 addr, u32 size);
	
	static VUTracer& get();
	
	std::atomic<int> trace_index { -1 };
	FILE* log_file = nullptr;
private:
	void beginTraceSession();
	void endTraceSession();
	void beginTrace();
	void endTrace();
	
	void pushLastPacket();

	VUTraceStatus status = VUTRACESTATUS_DISABLED;
	
	FILE* trace_file = nullptr;
	bool has_output_instructions = false;
	
	u32 read_addr = 0, read_size = 0;
	u32 write_addr = 0, write_size = 0;
	
	bool last_regs_populated = false;
	VURegs* last_regs;
	bool last_memory_populated = false;
	u8 last_memory[16 * 1024];
};
