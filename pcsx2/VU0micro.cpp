// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// This module contains code shared by both the dynarec and interpreter versions
// of the VU0 micro.

#include "Common.h"
#include "VUmicro.h"

#include <cmath>

using namespace R5900;

// This is called by the COP2 as per the CTC instruction
void vu0ResetRegs()
{
	VU0.VI[REG_VPU_STAT].UL &= ~0xff; // stop vu0
	VU0.VI[REG_FBRST].UL &= ~0xff; // stop vu0
	vif0Regs.stat.VEW = false;
}

static __fi u32 vu0DenormalizeMicroStatus(u32 nstatus)
{
	// from mVUallocSFLAGd()
	return ((nstatus >> 3) & 0x18u) | ((nstatus >> 11) & 0x1800u) | ((nstatus >> 14) & 0x3cf0000u);
}

static __fi void vu0SetMicroFlags(u32* flags, u32 value)
{
#ifdef _M_X86
	_mm_store_si128(reinterpret_cast<__m128i*>(flags), _mm_set1_epi32(value));
#elif defined(_M_ARM64)
	vst1q_u32(flags, vdupq_n_u32(value));
#else
	flags[0] = flags[1] = flags[2] = flags[3] = value;
#endif
}

void vu0ExecMicro(u32 addr) {
	VUM_LOG("vu0ExecMicro %x", addr);

	if(VU0.VI[REG_VPU_STAT].UL & 0x1) {
		DevCon.Warning("vu0ExecMicro > Stalling for previous microprogram to finish");
		vu0Finish();
	}

	// Need to copy the clip flag back to the interpreter in case COP2 has edited it
	const u32 CLIP = VU0.VI[REG_CLIP_FLAG].UL;
	const u32 MAC = VU0.VI[REG_MAC_FLAG].UL;
	const u32 STATUS = VU0.VI[REG_STATUS_FLAG].UL;
	VU0.clipflag = CLIP;
	VU0.macflag = MAC;
	VU0.statusflag = STATUS;

	// Copy flags to micro instances, since they may be out of sync if COP2 has run.
	// We do this at program start time, because COP2 can't execute until the program has completed,
	// but long-running program may be interrupted so we can't do it at dispatch time.
	vu0SetMicroFlags(VU0.micro_clipflags, CLIP);
	vu0SetMicroFlags(VU0.micro_macflags, MAC);
	vu0SetMicroFlags(VU0.micro_statusflags, vu0DenormalizeMicroStatus(STATUS));

	VU0.VI[REG_VPU_STAT].UL &= ~0xFF;
	VU0.VI[REG_VPU_STAT].UL |=  0x01;
	VU0.cycle = cpuRegs.cycle;
	if ((s32)addr != -1) VU0.VI[REG_TPC].UL = addr & 0x1FF;

	CpuVU0->SetStartPC(VU0.VI[REG_TPC].UL << 3);
	_vuExecMicroDebug(VU0);
	CpuVU0->ExecuteBlock(1);
}
