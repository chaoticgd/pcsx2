// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DebuggerWindow.h"

#include "DockLayoutManager.h"

class DebuggerWindow : public QMainWindow
{
	Q_OBJECT

public:
	DebuggerWindow(QWidget* parent);
	~DebuggerWindow();

public slots:
	void onVMStateChanged();
	void onRunPause();
	void onStepInto();
	void onStepOver();
	void onStepOut();
	void onAnalyse();

protected:
	void showEvent(QShowEvent* event);
	void hideEvent(QHideEvent* event);

private:
	Ui::DebuggerWindow m_ui;
	QAction* m_actionRunPause;
	QAction* m_actionStepInto;
	QAction* m_actionStepOver;
	QAction* m_actionStepOut;

	DockLayoutManager m_dock_layout_manager;

	void setTabActiveStyle(BreakPointCpu toggledCPU);
};
