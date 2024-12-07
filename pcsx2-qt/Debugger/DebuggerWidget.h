// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <DockWidget.h>

#include <QtWidgets/QWidget>

class DebuggerWidget : public ads::CDockWidget
{
	Q_OBJECT

protected:
	DebuggerWidget(const QString& title, DebugInterface* cpu, QWidget* parent = nullptr);

	DebugInterface& cpu() const;

private:
	DebugInterface* m_cpu;
};
