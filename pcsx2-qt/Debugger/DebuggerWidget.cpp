// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWidget.h"

#include "common/Assertions.h"

DebuggerWidget::DebuggerWidget(const QString& title, DebugInterface* cpu, QWidget* parent)
	: ads::CDockWidget(title, parent)
	, m_cpu(cpu)
{
}

DebugInterface& DebuggerWidget::cpu() const
{
	pxAssertRel(m_cpu, "DebuggerWidget::cpu() called on object that doesn't have a CPU type set.");
	return *m_cpu;
}
