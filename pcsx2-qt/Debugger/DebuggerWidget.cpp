// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWidget.h"

#include "JsonValueWrapper.h"

#include "common/Assertions.h"

DebugInterface& DebuggerWidget::cpu() const
{
	pxAssertRel(m_cpu, "DebuggerWidget::cpu() called on object that doesn't have a CPU type set.");
	return *m_cpu;
}

void DebuggerWidget::setCpu(DebugInterface* cpu)
{
	m_cpu = cpu;
}

void DebuggerWidget::toJson(JsonValueWrapper& json)
{
	rapidjson::Value cpu_name;
	switch (cpu().getCpuType())
	{
		case BREAKPOINT_EE:
			cpu_name.SetString("EE");
			break;
		case BREAKPOINT_IOP:
			cpu_name.SetString("IOP");
			break;
		default:
			return;
	}

	json.value().AddMember("cpu", cpu_name, json.allocator());
}

void DebuggerWidget::fromJson(JsonValueWrapper& json)
{
}

void DebuggerWidget::applyMonospaceFont()
{
	// Easiest way to handle cross platform monospace fonts
	// There are issues related to TabWidget -> Children font inheritance otherwise
#if defined(WIN32)
	setStyleSheet(QStringLiteral("font: 10pt 'Lucida Console'"));
#elif defined(__APPLE__)
	setStyleSheet(QStringLiteral("font: 10pt 'Monaco'"));
#else
	setStyleSheet(QStringLiteral("font: 10pt 'Monospace'"));
#endif
}

DebuggerWidget::DebuggerWidget(DebugInterface* cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
{
}
