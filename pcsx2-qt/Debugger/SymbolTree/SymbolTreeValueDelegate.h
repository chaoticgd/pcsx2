// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include "DebugTools/SymbolGuardian.h"

// This manages the editor widgets in the symbol trees.
class SymbolTreeValueDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SymbolTreeValueDelegate(
		const SymbolGuardian& guardian,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	// Without this, setModelData would only be called when a combo box was
	// deselected rather than when an option was picked.
	void onComboBoxIndexChanged(int index);

	const SymbolGuardian& m_guardian;
};
