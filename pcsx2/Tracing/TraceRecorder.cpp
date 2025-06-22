// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "TraceRecorder.h"

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Threading.h"

#include "Zydis/Zydis.h"

#include <sched.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <dlfcn.h>
#include <immintrin.h>

#ifdef PXTRACE_SUPPORTED

Tracing::TraceRecorder Tracing::g_recorder;

struct TracedGlobal
{
	const char* name;
	char* buffer;
	u32 offset;
	u32 size;
};

static std::vector<TracedGlobal> s_traced_globals;
static u32 s_traced_globals_size;
static ZydisDecoder s_decoder;

Tracing::TraceRecorder::TraceRecorder()
{
	ZydisDecoderInit(&s_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
}

Tracing::TraceRecorder::~TraceRecorder()
{
	if (m_buffer)
		free(m_buffer);
}

bool Tracing::TraceRecorder::BeginTrace(Error* error)
{
	struct sigaction action;
	action.sa_sigaction = HandleSigTrap;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTRAP, &action, nullptr) != 0)
	{
		Error::SetString(error, "Failed to setup signal handler.");
		return false;
	}

	if (m_buffer)
		free(m_buffer);

	m_buffer = static_cast<char*>(malloc(_256mb));
	m_buffer_size = _1gb;
	m_top = 0;

	if (!m_thread.Start(_256kb, RunDebugLoop, static_cast<void*>(this), error))
	{
		free(m_buffer);
		return false;
	}

	return true;
}

void Tracing::TraceRecorder::EndTrace(Error* error)
{
	m_thread.Stop();
}

void Tracing::TraceRecorder::SaveTrace(std::string file_path)
{
	if (!FileSystem::WriteBinaryFile(file_path.c_str(), m_buffer, m_top))
	{
		Console.Error("Tracing: Failed to write trace file '%s'.", file_path.c_str());
	}
}

void Tracing::TraceRecorder::BeginEvent(EventType event, Channel channel)
{
	PacketHeader* header = PushPacket(PacketType::BEGIN_EVENT, sizeof(EventPacket));
	EventPacket* packet = reinterpret_cast<EventPacket*>(header->data);
}

void Tracing::TraceRecorder::EndEvent(EventType event, Channel channel)
{
}

void Tracing::TraceRecorder::PushPromise(u32 flags)
{
}

void Tracing::TraceRecorder::PopPromise()
{
}

void Tracing::TraceRecorder::RunDebugLoop(HostDebugInterface& debug, void* user)
{
	TraceRecorder* recorder = static_cast<TraceRecorder*>(user);
	Console.WriteLn("Tracer PID: %d", getpid());
	Console.WriteLn("Tracee PID: %d", debug.Tracee());

	recorder->SaveState();

	// Set the trap flag on all attached threads so that the signal handler
	// below is called after each instruction is executed.
	for (const auto& [tid, thread] : debug.Threads())
	{
		user_regs_struct regs;
		if (ptrace(PTRACE_GETREGS, tid, nullptr, &regs) == -1)
		{
			Console.Error("ptrace(PTRACE_GETREGS)");
			return;
		}

		regs.eflags |= 0x100;

		if (ptrace(PTRACE_SETREGS, tid, nullptr, &regs) == -1)
		{
			Console.Error("ptrace(PTRACE_SETREGS)");
			return;
		}
	}

	// Resume all the threads until we get an event to handle.
	for (const auto& [tid, thread] : debug.Threads())
	{
		if (ptrace(PTRACE_CONT, tid, nullptr, nullptr) == -1)
		{
			Console.Error("ptrace(PTRACE_CONT)");
			return;
		}
	}

	while (HostDebugEvent* event = debug.WaitForEvent())
	{
		switch (event->type)
		{
			case HostDebugEvent::THREAD_CREATED:
			{
				debug.OnThreadCreated(event);
				break;
			}
			case HostDebugEvent::THREAD_EXITED:
			{
				debug.OnThreadExited(event);
				break;
			}
		}
	}
}

void Tracing::TraceRecorder::HandleSigTrap(int sig, siginfo_t* info, void* ucontext)
{
	ucontext_t* context = static_cast<ucontext_t*>(ucontext);

	// sausage roll
	u64 rip = context->uc_mcontext.gregs[REG_RIP];

	ThreadState& thread = g_recorder.m_threads[gettid()];

	if (thread.last_instruction_accessed_memory)
	{
		u64 new_value = *(u8*)thread.address;

		// Check if the memory access was to any of the buffers we're interested
		// in. If it was, log the address as well as the before and after value.
		u32 offset;
		if (TranslateHostAddressToOffset(thread.address, &offset))
		{
			PacketHeader* packet = g_recorder.PushPacket(PacketType::WRITE, sizeof(PacketHeader) + 4 + 8 + 8);
			*(u32*)&packet->data[0] = offset;
			*(u32*)&packet->data[4] = thread.old_value;
			*(u32*)&packet->data[4 + 8] = new_value;
		}

		thread.last_instruction_accessed_memory = false;
	}

	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
	ZydisDecoderDecodeFull(&s_decoder,
		reinterpret_cast<void*>(rip), 15, &instruction, operands);
	for (u8 i = 0; i < instruction.operand_count; i++)
	{
		// HACK: Ignore segment registers since we can't calculate the address
		// for them. On x64 Linux fs is used for thread local storage.
		if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY &&
			operands[i].mem.type == ZYDIS_MEMOP_TYPE_MEM &&
			operands[i].mem.segment == ZYDIS_REGISTER_NONE)
		{
			ZydisDisassembledInstruction dis;
			ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, rip, reinterpret_cast<void*>(rip), 15, &dis);

			ZydisRegisterContext zontext = {};
			LinuxContextToZydisContext(context, &zontext);

			ZyanU64 address;
			if ((ZYAN_SUCCESS(ZydisCalcAbsoluteAddressEx(&instruction, &operands[i], rip, &zontext, &address))))
			{
				// lookup symbol for function
				Dl_info symbol = {};
				dladdr(reinterpret_cast<void*>(rip), &symbol);

				if (symbol.dli_sname && symbol.dli_saddr)
					Console.WriteLn("%s+%lx --- %s+%lx --- %s --- %lx\n",
						symbol.dli_fname, rip - reinterpret_cast<u64>(symbol.dli_fbase), symbol.dli_sname, rip - reinterpret_cast<u64>(symbol.dli_saddr), dis.text, address);
				else
					Console.WriteLn("%s+%lx --- %p --- %s --- %lx\n",
						symbol.dli_fname, rip - reinterpret_cast<u64>(symbol.dli_fbase), rip, dis.text, address);

				thread.last_instruction_accessed_memory = true;
				thread.address = reinterpret_cast<char*>(address);
				thread.old_value = *(u8*)thread.address;
			}
		}
	}

	static const constexpr u64 maxcount = 200000000;

	static u64 icount = 0;
	if (icount % 1000000 == 0)
		Console.WriteLn("icount %ld %p %lu%\n", icount, context->uc_mcontext.gregs[REG_RIP], (icount * 100) / maxcount);
	icount++;

	if (icount > maxcount)
	{
		Tracing::g_recorder.SaveTrace("/tmp/trace");
		Tracing::g_recorder.EndTrace();
	}
}

void Tracing::TraceRecorder::SaveState()
{
	PacketHeader* packet = PushPacket(PacketType::SAVE_STATE, sizeof(PacketHeader) + s_traced_globals_size);
	for (const TracedGlobal& global : s_traced_globals)
		memcpy(packet->data + global.offset, global.buffer, global.size);
}

Tracing::PacketHeader* Tracing::TraceRecorder::PushPacket(PacketType type, u32 size)
{
	u32 offset;
	u32 aligned_offset;
	do
	{
		offset = m_top;
		aligned_offset = Common::AlignUpPow2(offset, 4);
	} while (!m_top.compare_exchange_weak(offset, aligned_offset + size));

	if (m_top > m_buffer_size)
		abort();

	PacketHeader* header = reinterpret_cast<PacketHeader*>(m_buffer + aligned_offset);
	header->type = type;
	header->size = size;
	return header;
}

void Tracing::RegisterGlobal(const char* name, char* buffer, size_t size)
{
	TracedGlobal& global = s_traced_globals.emplace_back();
	global.name = name;
	global.buffer = buffer;
	global.offset = Common::AlignUpPow2(s_traced_globals_size, 16);
	global.size = static_cast<u32>(size);

	s_traced_globals_size = global.offset + size;
}

bool Tracing::TranslateHostAddressToOffset(char* source, u32* destination)
{
	for (const TracedGlobal& global : s_traced_globals)
	{
		if (source >= global.buffer && source < global.buffer + global.size)
		{
			*destination = global.offset + static_cast<u32>(source - global.buffer);
			return true;
		}
	}

	return false;
}

void Tracing::LinuxContextToZydisContext(ucontext_t* source, ZydisRegisterContext* destination)
{
	// General Purpose Registers
	destination->values[ZYDIS_REGISTER_RAX] = source->uc_mcontext.gregs[REG_RAX];
	destination->values[ZYDIS_REGISTER_RCX] = source->uc_mcontext.gregs[REG_RCX];
	destination->values[ZYDIS_REGISTER_RDX] = source->uc_mcontext.gregs[REG_RDX];
	destination->values[ZYDIS_REGISTER_RBX] = source->uc_mcontext.gregs[REG_RBX];
	destination->values[ZYDIS_REGISTER_RSP] = source->uc_mcontext.gregs[REG_RSP];
	destination->values[ZYDIS_REGISTER_RBP] = source->uc_mcontext.gregs[REG_RBP];
	destination->values[ZYDIS_REGISTER_RSI] = source->uc_mcontext.gregs[REG_RSI];
	destination->values[ZYDIS_REGISTER_RDI] = source->uc_mcontext.gregs[REG_RDI];
	destination->values[ZYDIS_REGISTER_R8] = source->uc_mcontext.gregs[REG_R8];
	destination->values[ZYDIS_REGISTER_R9] = source->uc_mcontext.gregs[REG_R9];
	destination->values[ZYDIS_REGISTER_R10] = source->uc_mcontext.gregs[REG_R10];
	destination->values[ZYDIS_REGISTER_R11] = source->uc_mcontext.gregs[REG_R11];
	destination->values[ZYDIS_REGISTER_R12] = source->uc_mcontext.gregs[REG_R12];
	destination->values[ZYDIS_REGISTER_R13] = source->uc_mcontext.gregs[REG_R13];
	destination->values[ZYDIS_REGISTER_R14] = source->uc_mcontext.gregs[REG_R14];
	destination->values[ZYDIS_REGISTER_R15] = source->uc_mcontext.gregs[REG_R15];

	// Instruction Pointer
	destination->values[ZYDIS_REGISTER_RIP] = source->uc_mcontext.gregs[REG_RIP];

	// TODO: This doesn't work currently.
	// Zydis wants the addresses the segment registers point to rather than the
	// values of the segment registers themselves, hence this sillieness. It
	// appears that on x64 Linux they're used by pthread and stay the same over
	// time (hopefully).
	destination->values[ZYDIS_REGISTER_FS] = _readfsbase_u64();
	destination->values[ZYDIS_REGISTER_GS] = _readgsbase_u64();

	//ZyanU64 cs = 420, fs = 420, gs = 420;
	//asm("mov %%cs:(0),%0\n"
	//	"mov %%fs:(0),%1\n"
	//	"mov %%gs:(0),%2\n"
	//	: "=r"(cs, fs, gs)
	//	:
	//	: "eax");
	//
	//Console.WriteLn("%lx %lx %lx || %lx %lx %lx", cs, fs, gs,
	//	source->uc_mcontext.gregs[REG_CSGSFS] & 0xffff,
	//	(source->uc_mcontext.gregs[REG_CSGSFS] >> 32) & 0xffff,
	//	(source->uc_mcontext.gregs[REG_CSGSFS] >> 16) & 0xffff);
	//
	//destination->values[ZYDIS_REGISTER_CS] = cs;
	//destination->values[ZYDIS_REGISTER_FS] = fs;
	//destination->values[ZYDIS_REGISTER_GS] = gs;

	// Segment Registers
	//destination->values[ZYDIS_REGISTER_CS] = source->uc_mcontext.gregs[REG_CSGSFS] & 0xffff;
	//destination->values[ZYDIS_REGISTER_FS] = (source->uc_mcontext.gregs[REG_CSGSFS] >> 32) & 0xffff;
	//destination->values[ZYDIS_REGISTER_GS] = (source->uc_mcontext.gregs[REG_CSGSFS] >> 16) & 0xffff;
}

#endif
