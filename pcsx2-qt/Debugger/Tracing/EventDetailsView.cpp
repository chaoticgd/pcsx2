// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "EventDetailsView.h"

EventDetailsView::EventDetailsView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, NO_DEBUGGER_FLAGS)
{
	m_ui.setupUi(this);
}
