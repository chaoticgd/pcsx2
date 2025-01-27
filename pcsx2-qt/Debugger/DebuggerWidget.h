// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>

inline void not_yet_implemented()
{
	abort();
}

class JsonValueWrapper;

class DebuggerWidget : public QWidget
{
	Q_OBJECT

public:
	DebugInterface& cpu() const;
	void setCpu(DebugInterface* cpu);

	virtual void toJson(JsonValueWrapper& json);
	virtual void fromJson(JsonValueWrapper& json);

	void applyMonospaceFont();

	size_t widgetDescriptionIndex()
	{
		return m_widget_description_index;
	}

	void setWidgetDescriptionIndex(size_t index)
	{
		m_widget_description_index = index;
	}

	QString uniqueName;

protected:
	DebuggerWidget(DebugInterface* cpu, QWidget* parent = nullptr);

private:
	DebugInterface* m_cpu;
	size_t m_widget_description_index = SIZE_MAX;
};
