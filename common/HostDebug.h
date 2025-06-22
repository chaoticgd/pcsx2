// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#ifdef __linux__

#include "Error.h"
#include "Pcsx2Defs.h"

#include <sched.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <condition_variable>
#include <functional>
#include <map>

using ProcessID = pid_t;
using ThreadID = pid_t;

class HostDebugInterface;

// Create a thread in a new thread group so we can ptrace the other threads.
class HostDebugThread
{
public:
	using Callback = std::function<void(HostDebugInterface& debug, void* user)>;

	HostDebugThread();
	~HostDebugThread();

	HostDebugThread(const HostDebugThread& rhs) = delete;
	HostDebugThread(HostDebugThread&& rhs) = delete;
	HostDebugThread& operator=(const HostDebugThread& rhs) = delete;
	HostDebugThread& operator=(HostDebugThread&& rhs) = delete;

	bool Start(size_t stack_size, Callback callback, void* user, Error* error = nullptr);
	void Stop();

private:
	static int RunThread(void* arg);

	bool m_started = false;
	Callback m_callback;
	void* m_user = nullptr;
	ProcessID m_tracee = 0;
	ProcessID m_tracer = 0;
	std::atomic_bool m_interrupt = false;

	std::mutex m_permission_mutex;
	std::condition_variable m_permission_condition;
	bool m_permission = false;

	std::mutex m_attached_mutex;
	std::condition_variable m_attached_condition;
	bool m_attached = false;
};

struct HostDebugEvent
{
	enum Type
	{
		THREAD_CREATED,
		THREAD_EXITED
	};

	ThreadID tid;
	int status;
	Type type;
};

// Wrapper over ptrace, used to simplify the process of debugging multiple
// threads, since each thread has to be attached to and managed separately.
class HostDebugInterface
{
public:
	struct Thread
	{
		int status;
	};

	HostDebugInterface(ProcessID tracee, const std::atomic_bool* interrupt);
	~HostDebugInterface();

	__fi ProcessID Tracee() { return m_tracee; }
	__fi bool Interrupted() { return m_interrupt && *m_interrupt; }
	__fi const std::map<ThreadID, Thread>& Threads() { return m_threads; }

	bool Attach(Error* error = nullptr);
	bool Detach(Error* error = nullptr);

	HostDebugEvent* WaitForEvent();
	void OnThreadCreated(HostDebugEvent* event);
	void OnThreadExited(HostDebugEvent* event);

private:
	bool AttachToThread(ThreadID thread, Error* error = nullptr);
	bool DetachFromThread(ThreadID thread, Error* error = nullptr);
	int WaitPID(pid_t pid, int* stat_loc, int options, Error* error = nullptr);

	ProcessID m_tracee;
	const std::atomic_bool* m_interrupt;
	bool m_attached = false;
	std::map<ThreadID, Thread> m_threads;

	HostDebugEvent m_event;
};

#endif
