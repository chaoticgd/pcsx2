/*
	vutrace - Hacky VU tracer/debugger.
	Copyright (C) 2022 chaoticgd

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

#include "VUtracer.h"

#include "PrecompiledHeader.h"
#include "Common.h"
#include "Vif.h"
#include "Vif_Dma.h"
#include "newVif.h"
#include "GS.h"
#include "Gif.h"
#include "MTVU.h"
#include "Gif_Unit.h"
#include "DebugTools/Debug.h"

VUTracer::VUTracer() {
	last_regs = new VURegs;
}

void VUTracer::onTraceMenuItemClicked() {
	if(status == VUTRACESTATUS_DISABLED) {
		status = VUTRACESTATUS_WAITING;
	}
}

void VUTracer::onVsync() {
	if(status == VUTRACESTATUS_WAITING) {
		status = VUTRACESTATUS_TRACING;
		beginTraceSession();
	} else if(status == VUTRACESTATUS_TRACING) {
		status = VUTRACESTATUS_DISABLED;
		endTraceSession();
	}
}

void VUTracer::onVif1DmaSendChain(u32 tadr) {
	
}

void VUTracer::onVifDmaTag(u32 madr, u64 dma_tag) {
	
}

void VUTracer::onVu1ExecMicro(u32 pc) {
	if(status == VUTRACESTATUS_TRACING) {
		endTrace();
		beginTrace();
	}
}

void VUTracer::onInstructionExecuted(VURegs* regs) {
	if(status == VUTRACESTATUS_TRACING) {
		pushLastPacket();
		
		// Only write the microcode out once per file.
		if(!has_output_instructions) {
			fputc(VUTRACE_SETINSTRUCTIONS, trace_file);
			fwrite(regs->Micro, VU1_PROGSIZE, 1, trace_file);
			has_output_instructions = true;
		}
		
		// Only write out the registers that have changed.
		if(!last_regs_populated) {
			fputc(VUTRACE_SETREGISTERS, trace_file);
			fwrite(&regs->VF, sizeof(regs->VF), 1, trace_file);
			fwrite(&regs->VI, sizeof(regs->VI), 1, trace_file);
			fwrite(&regs->ACC, sizeof(regs->ACC), 1, trace_file);
			fwrite(&regs->q, sizeof(regs->q), 1, trace_file);
			fwrite(&regs->p, sizeof(regs->p), 1, trace_file);
			memcpy(last_regs, regs, sizeof(VURegs));
			last_regs_populated = true;
		} else {
			// Floating point registers.
			for(u8 i = 0; i < 32; i++) {
				if(memcmp(&last_regs->VF[i], &regs->VF[i], 16) != 0) {
					fputc(VUTRACE_PATCHREGISTER, trace_file);
					u8 register_index = i;
					fwrite(&register_index, 1, 1, trace_file);
					fwrite(&regs->VF[i], 16, 1, trace_file);
					memcpy(&last_regs->VF[i], &regs->VF[i], 16);
				}
			}
			
			// Integer registers.
			for(u8 i = 0; i < 32; i++) {
				if(memcmp(&last_regs->VI[i], &regs->VI[i], 16) != 0) {
					fputc(VUTRACE_PATCHREGISTER, trace_file);
					u8 register_index = 32 + i;
					fwrite(&register_index, 1, 1, trace_file);
					fwrite(&regs->VI[i], 16, 1, trace_file);
					memcpy(&last_regs->VI[i], &regs->VI[i], 16);
				}
			}
			
			// Other registers.
			if(memcmp(&last_regs->ACC, &regs->ACC, 16) != 0) {
				fputc(VUTRACE_PATCHREGISTER, trace_file);
				u8 register_index = 64;
				fwrite(&register_index, 1, 1, trace_file);
				fwrite(&regs->ACC, 16, 1, trace_file);
				memcpy(&last_regs->ACC, &regs->ACC, 16);
			}
			if(memcmp(&last_regs->q, &regs->q, 16) != 0) {
				fputc(VUTRACE_PATCHREGISTER, trace_file);
				u8 register_index = 65;
				fwrite(&register_index, 1, 1, trace_file);
				fwrite(&regs->q, 16, 1, trace_file);
				memcpy(&last_regs->q, &regs->q, 16);
			}
			if(memcmp(&last_regs->p, &regs->p, 16) != 0) {
				fputc(VUTRACE_PATCHREGISTER, trace_file);
				u8 register_index = 66;
				fwrite(&register_index, 1, 1, trace_file);
				fwrite(&regs->p, 16, 1, trace_file);
				memcpy(&last_regs->p, &regs->p, 16);
			}
		}
		
		// Only write out the values from memory that have changed.
		if(!last_memory_populated) {
			fputc(VUTRACE_SETMEMORY, trace_file);
			fwrite(regs->Mem, VU1_MEMSIZE, 1, trace_file);
			memcpy(last_memory, regs->Mem, VU1_MEMSIZE);
			last_memory_populated = true;
		} else {
			for(u32 i = 0; i < VU1_MEMSIZE; i += 4) {
				if(memcmp(&last_memory[i], &regs->Mem[i], 4) != 0) {
					fputc(VUTRACE_PATCHMEMORY, trace_file);
					fwrite(&i, 2, 1, trace_file);
					fwrite(&regs->Mem[i], 4, 1, trace_file);
					memcpy(&last_memory[i], &regs->Mem[i], 4);
				}
			}
		}
		
		// Keep track of which instructions are loads and stores.
		if(read_size > 0) {
			fputc(VUTRACE_LOADOP, trace_file);
			fwrite(&read_addr, sizeof(u32), 1, trace_file);
			fwrite(&read_size, sizeof(u32), 1, trace_file);
			read_size = 0;
		}
		
		if(write_size > 0) {
			fputc(VUTRACE_STOREOP, trace_file);
			fwrite(&write_addr, sizeof(u32), 1, trace_file);
			fwrite(&write_size, sizeof(u32), 1, trace_file);
			write_size = 0;
		}
	}
}

void VUTracer::onMemoryRead(u32 addr, u32 size) {
	read_addr = addr;
	read_size = size;
}

void VUTracer::onMemoryWrite(u32 addr, u32 size) {
	write_addr = addr;
	write_size = size;
}

void vutrace_log(const char* prefix, const char* fmt, ...) {
	FILE* log_file = VUTracer::get().log_file;
	if(log_file == nullptr) {
		return;
	}
	
	va_list args;
	va_start(args, fmt);
	fputs(prefix, log_file);
	vfprintf(log_file, fmt, args);
	fputc('\n', log_file);
	va_end(args);
}

VUTracer& VUTracer::get() {
	static VUTracer tracer;
	return tracer;
}

void VUTracer::beginTraceSession() {
	log_file = fopen("vutrace_output/LOG.txt", "wb");
	if(log_file == nullptr) {
		printf("[VUTrace] Fatal error: Cannot open log file for writing!\n");
		status = VUTRACE_DISABLED;
		return;
	}
	
	trace_index = -1;
	beginTrace();
}

void VUTracer::endTraceSession() {
	endTrace();
	trace_index = -1;
	fclose(log_file);
	log_file = nullptr;
	printf("[VUTrace] Trace session finished.\n");
}

void VUTracer::beginTrace() {
	int local_trace_index = trace_index++;
	
	char file_name[128];
	snprintf(file_name, 128, "vutrace_output/trace%06d.bin", local_trace_index);
	printf("[VUTrace] Tracing to %s\n", file_name);
	fprintf(log_file, "[VUTrace] ******************************** Tracing to %s ********************************\n", file_name);
	trace_file = fopen(file_name, "wb");
	if(trace_file == nullptr) {
		printf("[VUTrace] Fatal error: Cannot open trace file!\n");
	}
	
	// Write header.
	fputc('V', trace_file);
	fputc('U', trace_file);
	fputc('T', trace_file);
	fputc('R', trace_file);
	int format_version = 3;
	fwrite(&format_version, 4, 1, trace_file);
}

void VUTracer::endTrace() {
	pushLastPacket();
	fclose(trace_file);
	has_output_instructions = false;
	last_regs_populated = false;
	last_memory_populated = false;
}

void VUTracer::pushLastPacket() {
	if(ftell(trace_file) > 8) {
		fputc(VUTRACE_PUSHSNAPSHOT, trace_file);
	}
}
