// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "EventTimelineView.h"

EventTimelineView::EventTimelineView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, NO_DEBUGGER_FLAGS)
	, m_base_model()
	, m_cached_model(m_base_model)
{
	m_ui.setupUi(this);

	m_ui.view->setModel(&m_cached_model);
}
