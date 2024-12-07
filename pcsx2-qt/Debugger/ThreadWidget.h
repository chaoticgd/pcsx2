// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ThreadWidget.h"

#include "ThreadModel.h"

#include "DebuggerWidget.h"

class ThreadWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	ThreadWidget(DebugInterface& cpu, QWidget* parent = nullptr);

	void onContextMenu(QPoint pos);
	void onDoubleClick(const QModelIndex& index);

private:
	Ui::ThreadWidget m_ui;

	ThreadModel m_model;
	QSortFilterProxyModel m_proxy_model;
};
