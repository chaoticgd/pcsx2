// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_EventTimelineView.h"

#include "Debugger/DebuggerView.h"
#include "Debugger/Tracing/TimelineView.h"
#include "Debugger/Tracing/TimelineModel.h"

class EventTimelineView final : public DebuggerView
{
	Q_OBJECT

public:
	EventTimelineView(const DebuggerViewParameters& parameters);

private:
	Ui::EventTimelineView m_ui;

	TimelineView* m_view;
	DemoTimelineModel m_model;
};
