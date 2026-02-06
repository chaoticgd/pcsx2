// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <common/Pcsx2Types.h>

#include <QtCore/qttranslation.h>

namespace DebuggerEvents
{
	struct Event
	{
		virtual ~Event() = default;
	};

	// Sent when a debugger view is first created, and subsequently broadcast to
	// all debugger views at regular intervals and when the state of the VM is
	// changed from the debugger.
	struct Refresh : Event
	{
	};

	// Go to the address in a disassembly or memory view and switch to that tab.
	struct GoToAddress : Event
	{
		enum Filter
		{
			NONE,
			DISASSEMBLER,
			MEMORY_VIEW
		};

		u32 address = 0;

		// Prevent the memory view from handling events for jumping to functions
		// and vice versa.
		Filter filter = NONE;

		bool switch_to_tab = true;

		static constexpr const char* ACTION_STRING = QT_TRANSLATE_NOOP("DebuggerEvents", "Go to in %1");
		static constexpr const char* ACTION_OVERFLOW_STRING = QT_TRANSLATE_NOOP("DebuggerEvents", "Go to in...");
	};

	// Add the address to the saved addresses list and switch to that tab.
	struct AddToSavedAddresses : Event
	{
		u32 address = 0;
		bool switch_to_tab = true;

		static constexpr const char* ACTION_STRING = QT_TRANSLATE_NOOP("DebuggerEvents", "Add to %1");
		static constexpr const char* ACTION_OVERFLOW_STRING = QT_TRANSLATE_NOOP("DebuggerEvents", "Add to...");
	};
} // namespace DebuggerEvents
