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

#include <QtCore/QAbstractItemModel>

#include "common/Pcsx2Types.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/ccc/analysis.h"

class DataInspectorModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	struct TreeNode
	{
		QString name;
		const ccc::ast::Node* type = nullptr;
		u32 address = 0;
		TreeNode* parent = nullptr;
		std::vector<std::unique_ptr<TreeNode>> children;
		bool childrenFetched = false;
	};

	enum Column
	{
		NAME = 0,
		TYPE = 1,
		VALUE = 2,
		COLUMN_COUNT = 3
	};

	DataInspectorModel(
		std::unique_ptr<TreeNode> initialRoot,
		const ccc::HighSymbolTable& symbolTable,
		const std::map<std::string, s32>& typeNameToDeduplicatedTypeIndex,
		QObject* parent = nullptr);

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	void fetchMore(const QModelIndex& parent) override;
	bool canFetchMore(const QModelIndex& parent) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

protected:
	bool nodeHasChildren(const ccc::ast::Node& type) const;
	QModelIndex indexFromNode(const TreeNode& node) const;
	QString typeToString(const ccc::ast::Node& type) const;

	std::unique_ptr<TreeNode> m_root;
	const ccc::HighSymbolTable& m_symbolTable;
	const std::map<std::string, s32>& m_typeNameToDeduplicatedTypeIndex;
};

const ccc::ast::Node& resolvePhysicalType(const ccc::ast::Node& type, const ccc::HighSymbolTable& symbolTable, const std::map<std::string, s32>& nameToType);
