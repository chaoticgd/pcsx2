/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include "Debugger/Models/DataInspectorModel.h"

class DataInspectorValueColumnDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	using TreeNode = DataInspectorModel::TreeNode;

	DataInspectorValueColumnDelegate(
		const ccc::HighSymbolTable& symbolTable,
		const std::map<std::string, s32>& typeNameToDeduplicatedTypeIndex,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	const ccc::HighSymbolTable& m_symbolTable;
	const std::map<std::string, s32>& m_typeNameToDeduplicatedTypeIndex;
};
