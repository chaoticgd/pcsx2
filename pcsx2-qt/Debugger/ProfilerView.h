// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ProfilerView.h"

#include "DebuggerView.h"
#include "ProfilerModel.h"

#include <chrono>

class ProfilerView : public DebuggerView
{
	Q_OBJECT

public:
	ProfilerView(const DebuggerViewParameters& parameters);
	~ProfilerView();

	void worker(size_t sample_count, std::chrono::nanoseconds duration, DebugInterface& cpu);

private:
	void shutdownWorker();

	Ui::ProfilerView m_ui;

	std::thread m_thread;
	std::atomic_bool m_interrupt;

	ProfilerModel* m_function_model;
	ProfilerModel* m_address_model;
};
