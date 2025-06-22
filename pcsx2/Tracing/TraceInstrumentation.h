// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "TraceRecorder.h"

#include "common/Pcsx2Defs.h"

// This is the file that should be incuded by code being traced to mark up
// blocks of memory that should be traced, and to generate events via
// instrumentation macros.
//
// These macros should be used instead of the trace recording functions directly
// so that they can be compiled out.

#if PXTRACE_SUPPORTED

// Mark up global variable definitions that you want to trace.
#ifdef _MSC_VER
#define PXTRACE_GLOBAL(name, definition) \
	definition; \
	PXTRACE_GENERATE_GLOBAL_REGISTRAR(name)
#else
#define PXTRACE_GLOBAL(name, definition) \
	definition; \
	PXTRACE_GENERATE_GLOBAL_REGISTRAR(name)
#endif

#define PXTRACE_GENERATE_GLOBAL_REGISTRAR(name) \
	struct TraceGlobalRegistrar_##name \
	{ \
		TraceGlobalRegistrar_##name() \
		{ \
			Tracing::RegisterGlobal(#name, reinterpret_cast<char*>(&name), sizeof(name)); \
		} \
	} g_trace_global_registrar_##name

// Push a begin event packet. Should be paired with a matching TRACE_END_EVENT
// invocation.
#define PXTRACE_BEGIN_EVENT(event, channel) \
	Tracing::g_recorder.BeginEvent(event, channel)

// Push a end event packet. Should be paired with a matching TRACE_BEGIN_EVENT
// invocation.
#define PXTRACE_END_EVENT(event, channel) \
	Tracing::g_recorder.EndEvent(event, channel)

// Push a begin event packet immediately and a matching end event packet when it
// goes out of scope.
#define PXTRACE_SCOPED_EVENT(event, channel) \
	Tracing::ScopedEvent scoped_event_##__COUNTER__(event, channel);

#define PXTRACE_PUSH_PROMISE(flags) \
	Tracing::g_recorder.PushPromise(flags);

#define PXTRACE_POP_PROMISE() \
	Tracing::g_recorder.PopPromise();

#define PXTRACE_SCOPED_PROMISE(flags) \
	Tracing::ScopedPromise scoped_promise_##__COUNTER__(flags);

#else

#define PXTRACE_GLOBAL(name, definition) definition
#define PXTRACE_BEGIN_EVENT(event, channel)
#define PXTRACE_END_EVENT(event, channel)
#define PXTRACE_SCOPED_EVENT(event, channel)
#define PXTRACE_PUSH_PROMISE(flags)
#define PXTRACE_POP_PROMISE()
#define PXTRACE_SCOPED_PROMISE(flags)

#endif
