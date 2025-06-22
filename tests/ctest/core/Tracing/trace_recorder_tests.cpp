// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Tracing/TraceInstrumentation.h"
#include <gtest/gtest.h>

#ifdef PXTRACE_SUPPORTED

PXTRACE_GLOBAL(a, int a);
PXTRACE_GLOBAL(b, int b);

TEST(TraceRecorder, DisassembleMov)
{
	
}

#endif
