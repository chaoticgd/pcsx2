// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_EventDetailsView.h"

#include "Debugger/DebuggerView.h"

class EventDetailsView final : public DebuggerView
{
	Q_OBJECT

public:
	EventDetailsView(const DebuggerViewParameters& parameters);

private:
	Ui::EventDetailsView m_ui;
};
