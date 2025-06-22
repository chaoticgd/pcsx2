// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "TraceFormat.h"

#include "common/Error.h"
#include "common/HostDebug.h"

#if defined(__linux__) && defined(__x86_64__)
#define PXTRACE_SUPPORTED 1
#else
#define PXTRACE_SUPPORTED 0
#endif

#if PXTRACE_SUPPORTED

typedef struct ZydisRegisterContext_ ZydisRegisterContext;

namespace Tracing
{
	enum PromiseFlags
	{
		// This thread doesn't make any promises about whether it will read or
		// write to a traced buffer.
		NO_PROMISES = 0,
		// This thread promises it won't read from a traced buffer.
		NO_READS = 1 << 1,
		// This thread promises it won't write to a traced buffer.
		NO_WRITES = 1 << 2
	};

	class TraceRecorder
	{
	public:
		TraceRecorder();
		~TraceRecorder();

		bool BeginTrace(Error* error = nullptr);
		void EndTrace(Error* error = nullptr);
		void SaveTrace(std::string file_path);

		// See PXTRACE_BEGIN_EVENT.
		void BeginEvent(EventType event, Channel channel);

		// See PXTRACE_END_EVENT.
		void EndEvent(EventType event, Channel channel);

		// See PXTRACE_PUSH_PROMISE.
		void PushPromise(u32 flags);

		// See PXTRACE_POP_PROMISE.
		void PopPromise();

	private:
		static void RunDebugLoop(HostDebugInterface& debug, void* user);
		static void HandleSigTrap(int sig, siginfo_t* info, void* ucontext);

		void SaveState();

		PacketHeader* PushPacket(PacketType type, u32 size);

		char* m_buffer = nullptr;
		size_t m_buffer_size = 0;
		std::atomic_uint32_t m_top = 0;

		HostDebugThread m_thread;

		struct ThreadState
		{
			bool last_instruction_accessed_memory = false;
			char* address;
			u64 old_value;
		};

		std::map<ThreadID, ThreadState> m_threads;
	};

	extern TraceRecorder g_recorder;

	// See PXTRACE_SCOPED_EVENT.
	class ScopedEvent
	{
	public:
		ScopedEvent(EventType event, Channel channel)
			: m_event(event)
			, m_channel(channel)
		{
			g_recorder.BeginEvent(event, channel);
		}

		~ScopedEvent()
		{
			g_recorder.EndEvent(m_event, m_channel);
		}

	private:
		EventType m_event;
		Channel m_channel;
	};

	// See PXTRACE_SCOPED_PROMISE.
	class ScopedPromise
	{
	public:
		ScopedPromise(u32 flags)
		{
			g_recorder.PushPromise(flags);
		}

		~ScopedPromise()
		{
			g_recorder.PopPromise();
		}
	};


	// Register a global variable to be traced. Should be called automatically
	// by TRACE_GLOBAL.
	void RegisterGlobal(const char* name, char* buffer, size_t size);

	bool TranslateHostAddressToOffset(char* source, u32* destination);

	void LinuxContextToZydisContext(ucontext_t* source, ZydisRegisterContext* destination);
}; // namespace Tracing

#endif
