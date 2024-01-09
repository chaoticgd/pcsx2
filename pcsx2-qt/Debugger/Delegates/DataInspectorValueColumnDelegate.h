// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include "Debugger/Models/DataInspectorModel.h"

class DataInspectorValueColumnDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	DataInspectorValueColumnDelegate(
		const SymbolGuardian& guardian,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	const SymbolGuardian& m_guardian;
};
