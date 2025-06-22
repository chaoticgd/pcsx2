// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "HostDebug.h"

#ifdef __linux__

#include "Assertions.h"
#include "Console.h"
#include "Path.h"
#include "FileSystem.h"

#include <linux/prctl.h>
#include <sys/prctl.h>

#include <optional>

static std::optional<std::vector<ThreadID>> EnumerateThreads(ProcessID process, Error* error = nullptr);

struct HostDebugLogger
{
	HostDebugLogger(const char* name)
		: m_name(name)
	{
		Console.WriteLn("Entered %s", m_name);
	}

	~HostDebugLogger()
	{
		Console.WriteLn("Exited %s", m_name);
	}

private:
	const char* m_name;
};

// Turns out you can't use a debugger if you're using ptrace yourself, so printf
// debugging it is!
#define HOST_DEBUG_LOG(name) HostDebugLogger l(name)

HostDebugThread::HostDebugThread()
{
	HOST_DEBUG_LOG("HostDebugThread::HostDebugThread");
}

HostDebugThread::~HostDebugThread()
{
	HOST_DEBUG_LOG("HostDebugThread::~HostDebugThread");

	if (m_started)
		Stop();
}

bool HostDebugThread::Start(size_t stack_size, Callback callback, void* user, Error* error)
{
	HOST_DEBUG_LOG("HostDebugThread::Start");

	if (m_started)
	{
		Error::SetString(error, "Tracer thread already started.");
		return false;
	}

	// Fail early if we don't have permission to use ptrace.
	std::optional<std::string> ptrace_scope_str = FileSystem::ReadFileToString("/proc/sys/kernel/yama/ptrace_scope");
	if (ptrace_scope_str.has_value())
	{
		int ptrace_scope = atoi(ptrace_scope_str->c_str());
		if (ptrace_scope > 1)
		{
			Error::SetString(error,
				"PCSX2 doesn't have permission to attach to itself with ptrace. "
				"Try running: echo 1 | sudo tee /proc/sys/kernel/yama/ptrace_scope");
			return false;
		}
	}

	m_started = true;
	m_callback = callback;
	m_user = user;
	m_tracee = getpid();

	char* stack = static_cast<char*>(malloc(stack_size));
	if (!stack)
	{
		Error::SetErrno(error, errno);
		return false;
	}

	m_interrupt = false;
	m_permission = false;
	m_attached = false;

	int flags = CLONE_VM;

	// Create a new thread group so we can attach to the rest of the threads.
	// See: https://yarchive.net/comp/linux/ptrace_self_attach.html
	int pid;
	{
		HOST_DEBUG_LOG("HostDebugThread::Start(clone)");
		pid = clone(RunThread, static_cast<void*>(stack + stack_size), flags, static_cast<void*>(this));
		if (pid == -1)
		{
			m_started = false;
			free(stack);

			Error::SetErrno(error, errno);
			return false;
		}
	}

	// Give the child permission to attach. This is only relevant if running the
	// command "cat /proc/sys/kernel/yama/ptrace_scope" prints out 1.
	if (prctl(PR_SET_PTRACER, pid) != 0)
	{
		// TODO: cleanup

		Error::SetErrno(error, "prctl(PR_SET_PTRACER)", errno);
		return false;
	}

	// Tell the child they have permission to attach.
	{
		std::lock_guard lock(m_permission_mutex);
		m_permission = true;
	}
	m_permission_condition.notify_one();

	m_tracer = pid;

	// Wait until we've attached.
	{
		std::unique_lock lock(m_attached_mutex);
		m_attached_condition.wait(lock, [&]() { return m_attached; });
	}

	return true;
}

void HostDebugThread::Stop()
{
	HOST_DEBUG_LOG("HostDebugThread::Stop");

	pxAssert(m_started);
	m_started = false;

	// Tell the thread to exit.
	m_interrupt = true;

	// Wait for the thread to exit.
	int status;
	waitpid(m_tracer, &status, 0);
}

int HostDebugThread::RunThread(void* arg)
{
	HOST_DEBUG_LOG("HostDebugThread::RunThread");

	HostDebugThread* thread = static_cast<HostDebugThread*>(arg);
	HostDebugInterface debug(thread->m_tracee, &thread->m_interrupt);

	// Wait until we have permission to attach.
	{
		HOST_DEBUG_LOG("HostDebugThread::RunThread(permission wait)");
		std::unique_lock lock(thread->m_permission_mutex);
		thread->m_permission_condition.wait(lock, [&]() { return thread->m_permission; });
	}

	// Attach to all threads from the parent process.
	Error attach_error;
	if (!debug.Attach(&attach_error))
	{
		Console.Error("Failed to attach: %s", attach_error.GetDescription().c_str());
		return 1;
	}

	// Tell the parent we've attached.
	{
		std::lock_guard lock(thread->m_attached_mutex);
		thread->m_attached = true;
	}
	thread->m_attached_condition.notify_one();

	// Enter the main debugging loop.
	{
		HOST_DEBUG_LOG("HostDebugThread::RunThread(callback)");
		thread->m_callback(debug, thread->m_user);
	}

	// Detach from all attached threads.
	Error detach_error;
	if (!debug.Detach(&detach_error))
	{
		Console.Error("Failed to detach: %s", detach_error.GetDescription().c_str());
		return 1;
	}

	return 0;
}

// *****************************************************************************

HostDebugInterface::HostDebugInterface(ProcessID tracee, const std::atomic_bool* interrupt)
	: m_tracee(tracee)
	, m_interrupt(interrupt)
{
	HOST_DEBUG_LOG("HostDebugInterface::HostDebugInterface");
}

HostDebugInterface::~HostDebugInterface()
{
	HOST_DEBUG_LOG("HostDebugInterface::~HostDebugInterface");

	if (m_attached)
	{
		Error error;
		if (!Detach(&error))
			Console.Error("Failed to detach: %s", error.GetDescription().c_str());
	}
}

bool HostDebugInterface::Attach(Error* error)
{
	HOST_DEBUG_LOG("HostDebugInterface::Attach");

	pxAssert(!m_attached);

	// In order to handle the case where a thread is being spawned at the same
	// time as this function is running, repeatedly enumerate the list of
	// threads until there aren't any more we need to attach to. See:
	// https://github.com/eteran/edb-debugger/blob/eedd97a0c1dbdfde50f1c1ab617d5fd9e25049d9/plugins/DebuggerCore/unix/linux/DebuggerCore.cpp#L729
	bool attached;
	do
	{
		attached = false;

		std::optional<std::vector<ThreadID>> threads = EnumerateThreads(m_tracee, error);
		if (!threads.has_value())
			return false;

		for (ThreadID thread : *threads)
		{
			if (m_threads.contains(thread))
				continue;

			if (!AttachToThread(thread, error))
				return false;
		}
	} while (attached);

	m_attached = true;

	return true;
}

bool HostDebugInterface::Detach(Error* error)
{
	HOST_DEBUG_LOG("HostDebugInterface::Detach");

	pxAssert(m_attached);

	for (const auto& [tid, thread] : m_threads)
	{
		if (!DetachFromThread(tid, error))
			return false;
	}

	m_attached = false;
	m_threads.clear();

	return true;
}

HostDebugEvent* HostDebugInterface::WaitForEvent()
{
	if (m_interrupt && *m_interrupt)
		return nullptr;

	int status;
	ThreadID tid = waitpid(-1, &status, __WALL);
	m_event.tid = tid;
	m_event.status = status;

	auto thread = m_threads.find(tid);
	pxAssert(thread != m_threads.end());
	thread->second.status = status;

	if (WIFEXITED(status))
	{
		m_event.type = HostDebugEvent::THREAD_EXITED;
		return &m_event;
	}

	return nullptr;
}


void HostDebugInterface::OnThreadCreated(HostDebugEvent* event)
{
	HOST_DEBUG_LOG("HostDebugInterface::OnThreadCreated");
}

void HostDebugInterface::OnThreadExited(HostDebugEvent* event)
{
	HOST_DEBUG_LOG("HostDebugInterface::OnThreadExited");
}

bool HostDebugInterface::AttachToThread(ThreadID thread, Error* error)
{
	HOST_DEBUG_LOG("HostDebugInterface::AttachToThread");

	if (ptrace(PTRACE_ATTACH, thread, nullptr, nullptr) == -1)
	{
		Error::SetErrno(error, "ptrace(PTRACE_ATTACH) ", errno);
		return false;
	}

	m_threads.emplace(thread, Thread{});

	if (WaitPID(thread, NULL, __WALL, error) == -1)
		return false;

	if (ptrace(PTRACE_SETOPTIONS, thread, nullptr, PTRACE_O_TRACECLONE) == -1)
	{
		Error::SetErrno(error, "ptrace(PTRACE_SETOPTIONS) ", errno);
		return false;
	}

	return true;
}

bool HostDebugInterface::DetachFromThread(ThreadID thread, Error* error)
{
	HOST_DEBUG_LOG("HostDebugInterface::DetachFromThread");

	if (ptrace(PTRACE_DETACH, thread, 0, 0) == -1)
	{
		Error::SetErrno(error, "ptrace(PTRACE_DETACH) ", errno);
		return false;
	}

	return true;
}

int HostDebugInterface::WaitPID(pid_t pid, int* stat_loc, int options, Error* error)
{
	HOST_DEBUG_LOG("HostDebugInterface::WaitPID");

	auto iterator = m_threads.find(pid);
	pxAssert(iterator != m_threads.end());
	Thread& thread = iterator->second;

	int status;
	int result = waitpid(pid, &status, options);
	if (result == -1)
		Error::SetErrno(error, "waitpid ", errno);

	thread.status = status;
	if (stat_loc)
		*stat_loc = status;

	return result;
}

static std::optional<std::vector<ThreadID>> EnumerateThreads(ProcessID process, Error* error)
{
	HOST_DEBUG_LOG(__FUNCTION__);

	// Use the proc filesystem to enumerate threads. This is the method gdb and
	// edb both use. See:
	// https://sourceware.org/cgit/binutils-gdb/tree/gdb/nat/linux-osdata.c?id=7af3b05ce933624c1283331c11e49fea6ced9d96#n471
	// https://sourceware.org/cgit/binutils-gdb/tree/gdbserver/thread-db.cc?id=7af3b05ce933624c1283331c11e49fea6ced9d96#n739
	// https://github.com/eteran/edb-debugger/blob/eedd97a0c1dbdfde50f1c1ab617d5fd9e25049d9/plugins/DebuggerCore/unix/linux/DebuggerCore.cpp#L729

	std::string path = fmt::format("/proc/{}/task", process);

	FileSystem::FindResultsArray results;
	if (!FileSystem::FindFiles(path.c_str(), "*", FILESYSTEM_FIND_FOLDERS, &results))
	{
		Error::SetString(error, "Failed to enumerate tasks.");
		return std::nullopt;
	}

	std::vector<ThreadID> threads;
	for (const FILESYSTEM_FIND_DATA& entry : results)
	{
		std::string name(Path::GetFileName(entry.FileName));
		long long tid = atoll(name.c_str());
		if (tid != 0)
			threads.emplace_back(static_cast<ThreadID>(tid));
	}

	return threads;
}

#endif
